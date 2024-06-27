#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/x64frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  /* TODO: Put your lab5 code here */
  auto acc = level->frame_->F_allocLocal(escape);
  return new Access(level, acc);
}

class Cx {
public:
  PatchList trues_;
  PatchList falses_;
  tree::Stm *stm_;

  Cx(PatchList trues, PatchList falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    return this->exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(this->exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    tree::CjumpStm *jump_stm =
        new tree::CjumpStm(tree::RelOp::NE_OP, this->exp_,
                           new tree::ConstExp(0), nullptr, nullptr);
    return Cx(PatchList(std::list<temp::Label **>{(&(jump_stm->true_label_))}),
              PatchList(std::list<temp::Label **>{(&(jump_stm->false_label_))}),
              jump_stm);
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    return new tree::EseqExp(this->stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return this->stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    errormsg->Error(0, "cannot convert a NxExp to a Cx\n");
    return Cx(PatchList(std::list<temp::Label **>{}),
              PatchList(std::list<temp::Label **>{}), nullptr);
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(PatchList trues, PatchList falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}

  CxExp(temp::Label **trues, temp::Label **falses, tree::Stm *stm)
      : cx_(PatchList(std::list<temp::Label **>{trues}),
            PatchList(std::list<temp::Label **>{falses}), stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    auto r = temp::TempFactory::NewTemp();
    auto t = temp::LabelFactory::NewLabel();
    auto f = temp::LabelFactory::NewLabel();
    this->cx_.trues_.DoPatch(t);
    this->cx_.falses_.DoPatch(f);
    return new tree::EseqExp(
        new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),
        new tree::EseqExp(
            this->cx_.stm_,
            new tree::EseqExp(
                new tree::LabelStm(f),
                new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r),
                                                    new tree::ConstExp(0)),
                                  new tree::EseqExp(new tree::LabelStm(t),
                                                    new tree::TempExp(r))))));
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(this->UnEx());
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    return this->cx_;
  }
};

// 回溯获得静态链地址
tree::Exp *StaticLink(Level *current, Level *target) {
  tree::Exp *frame_ptr = new tree::TempExp(reg_manager->FramePointer());
  while (current != target) {
    frame_ptr = current->frame_->formal_list->at(0)->getT_EXP(frame_ptr);
    current = current->parent_;
  }
  return frame_ptr;
}

// 将已经分配了堆空间的record的一个field进行值初始化
tree::MoveStm *InitializeRecode(temp::Temp *record_temp, Exp *init_exp,
                                int index) {
  return new tree::MoveStm(
      new tree::MemExp(new tree::BinopExp(
          tree::BinOp::PLUS_OP, new tree::TempExp(record_temp),
          new tree::ConstExp(index * reg_manager->WordSize()))),
      init_exp->UnEx());
}

// IR树翻译的入口
void ProgTr::Translate() {
  /* TODO: Put your lab5 code here */
  auto ir_tree = this->absyn_tree_->Translate(
      this->venv_.get(), this->tenv_.get(), this->main_level_.get(), nullptr,
      this->errormsg_.get());
  frags->PushBack(
      frame::ProcEntryExit1(main_level_->frame_, ir_tree->exp_->UnNx()));
}

} // namespace tr

namespace absyn {

// 将抽象语法树中的Oper转换为tree中的BinOP
tree::BinOp op_tran_bin(Oper ab_op) {
  switch (ab_op) {
  case Oper::PLUS_OP: {
    return tree::BinOp::PLUS_OP;
  }
  case Oper::MINUS_OP: {
    return tree::BinOp::MINUS_OP;
  }
  case Oper::TIMES_OP: {
    return tree::BinOp::MUL_OP;
  }
  case Oper::DIVIDE_OP: {
    return tree::BinOp::DIV_OP;
  }
  }
}

// 将抽象语法树中的Oper转换为tree中的RelOp
tree::RelOp op_tran_rel(absyn::Oper ab_op) {
  switch (ab_op) {
  case Oper::EQ_OP: {
    return tree::RelOp::EQ_OP;
  }
  case Oper::NEQ_OP: {
    return tree::RelOp::NE_OP;
  }
  case Oper::LT_OP: {
    return tree::RelOp::LT_OP;
  }
  case Oper::LE_OP: {
    return tree::RelOp::LE_OP;
  }
  case Oper::GT_OP: {
    return tree::RelOp::GT_OP;
  }
  case Oper::GE_OP: {
    return tree::RelOp::GE_OP;
  }
  }
  return tree::RelOp::REL_OPER_COUNT;
}

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return this->root_->Translate(venv, tenv, level, label, errormsg);
}

// 变量语句，需要获取这个变量的访问用IR树
// 在找到变量后，根据其静态链进行回溯，最终找到这个变量的所在栈帧level
tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto var_entry = static_cast<env::VarEntry *>(venv->Look(this->sym_));
  return new tr::ExpAndTy(
      new tr::ExExp(var_entry->access_->access_->getT_EXP(
          tr::StaticLink(level, var_entry->access_->level_))),
      var_entry->ty_->ActualTy());
}

// record的field语句
tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // 翻译var，获取record的访问指针所需树和该record类型
  auto var_exp = this->var_->Translate(venv, tenv, level, label, errormsg);
  auto field_list =
      static_cast<type::RecordTy *>(var_exp->ty_)->fields_->GetList();
  // 逐个field匹配，在遇到匹配的field后停止，其offset和type将被记录
  int offset = 0;
  type::Ty *type = nullptr;
  for (auto field_item : field_list) {
    if (field_item->name_ == this->sym_) {
      type = field_item->ty_;
      break;
    }
    ++offset;
  }
  // record指针+offset获取所需field字段
  return new tr::ExpAndTy(
      new tr::ExExp(new tree::MemExp(new tree::BinopExp(
          tree::BinOp::PLUS_OP, var_exp->exp_->UnEx(),
          new tree::ConstExp(offset * reg_manager->WordSize())))),
      type);
}

// 数组元素语句
tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // 翻译var,作用同上
  auto var_exp = this->var_->Translate(venv, tenv, level, label, errormsg);
  auto subscript_exp =
      this->subscript_->Translate(venv, tenv, level, label, errormsg);
  return new tr::ExpAndTy(
      new tr::ExExp(new tree::MemExp(new tree::BinopExp(
          tree::BinOp::PLUS_OP, var_exp->exp_->UnEx(),
          new tree::BinopExp(tree::BinOp::MUL_OP, subscript_exp->exp_->UnEx(),
                             new tree::ConstExp(reg_manager->WordSize()))))),
      // ArrayTy内部的ty对象是其单个元素的类型
      static_cast<type::ArrayTy *>(var_exp->ty_)->ty_);
}

// Var语句，直接翻译即可，对其的实际翻译交由上面三个语句
tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return this->var_->Translate(venv, tenv, level, label, errormsg);
}

// Nil语句，返回空即可
tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::NilTy::Instance());
}

// Int语句，返回语句的实际数字
tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(this->val_)),
                          type::IntTy::Instance());
}

// String语句，为string创建新的标签，并在frag中加入“字符串常量”对象
// 后续可以用此标签访问String
tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto string_label = temp::LabelFactory::NewLabel();
  frags->PushBack(new frame::StringFrag(string_label, this->str_));
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(string_label)),
                          type::StringTy::Instance());
}

// 函数调用语句
tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto function_entry = static_cast<env::FunEntry *>(venv->Look(this->func_));
  auto function_level = function_entry->level_;
  auto arg_list = this->args_->GetList();
  // 将参数逐个翻译，并推入explist
  auto arg_exp_list = new tree::ExpList();
  for (auto arg_item : arg_list) {
    auto arg_exp_item = arg_item->Translate(venv, tenv, level, label, errormsg);
    arg_exp_list->Append(arg_exp_item->exp_->UnEx());
  }
  tree::Exp *call_exp;
  // 如果level为nullptr,证明其为一个外部调用的库函数，否则是程序创建的函数
  if (function_level == nullptr) {
    call_exp = frame::ExternalCall(this->func_->Name(), arg_exp_list);
  } else {
    // 第一参数为静态链
    arg_exp_list->Insert(
        tr::StaticLink(level, function_entry->level_->parent_));
    call_exp = new tree::CallExp(new tree::NameExp(function_entry->label_),
                                 arg_exp_list);
  }
  return new tr::ExpAndTy(new tr::ExExp(call_exp), function_entry->result_);
}

// 二元运算语句
tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto left_tree = this->left_->Translate(venv, tenv, level, label, errormsg);
  auto right_tree = this->right_->Translate(venv, tenv, level, label, errormsg);
  tr::Exp *result_exp;
  switch (this->oper_) {
    // 数字运算
  case absyn::Oper::PLUS_OP:
  case absyn::Oper::MINUS_OP:
  case absyn::Oper::TIMES_OP:
  case absyn::Oper::DIVIDE_OP: {
    result_exp = new tr::ExExp(new tree::BinopExp(op_tran_bin(this->oper_),
                                                  left_tree->exp_->UnEx(),
                                                  right_tree->exp_->UnEx()));
    break;
  }
  // 逻辑运算
  case absyn::Oper::LT_OP:
  case absyn::Oper::LE_OP:
  case absyn::Oper::GT_OP:
  case absyn::Oper::GE_OP: {
    auto jump_stm =
        new tree::CjumpStm(op_tran_rel(this->oper_), left_tree->exp_->UnEx(),
                           right_tree->exp_->UnEx(), nullptr, nullptr);
    result_exp = new tr::CxExp(&(jump_stm->true_label_),
                               &(jump_stm->false_label_), jump_stm);
    break;
  }
  // 逻辑运算，但考虑string也要参与比较的部分（需要外部函数）
  case absyn::Oper::EQ_OP:
  case absyn::Oper::NEQ_OP: {
    if (left_tree->ty_->IsSameType(type::StringTy::Instance())) {
      auto string_exp_list =
          new tree::ExpList{left_tree->exp_->UnEx(), right_tree->exp_->UnEx()};
      auto jump_stm = new tree::CjumpStm(
          tree::RelOp::EQ_OP,
          frame::ExternalCall("string_equal", string_exp_list),
          new tree::ConstExp(1), nullptr, nullptr);
      result_exp = new tr::CxExp(&(jump_stm->true_label_),
                                 &(jump_stm->false_label_), jump_stm);
    } else {
      auto jump_stm =
          new tree::CjumpStm(op_tran_rel(this->oper_), left_tree->exp_->UnEx(),
                             right_tree->exp_->UnEx(), nullptr, nullptr);
      result_exp = new tr::CxExp(&(jump_stm->true_label_),
                                 &(jump_stm->false_label_), jump_stm);
    }
    break;
  }
  // AND和OR逻辑运算，由于存在中途中断机制，需要做两层
  // 左（T:标签mid/F:标签f）AND 右（T:标签t/F:标签f）
  // 左jump语句（左判定句是否为true） mid标签 右jump语句（右判定句是否为true）
  case absyn::AND_OP: {
    auto mid_label = temp::LabelFactory::NewLabel();
    auto left_cjump =
        new tree::CjumpStm(tree::RelOp::NE_OP, left_tree->exp_->UnEx(),
                           new tree::ConstExp(0), mid_label, nullptr);
    auto right_cjump =
        new tree::CjumpStm(tree::RelOp::NE_OP, right_tree->exp_->UnEx(),
                           new tree::ConstExp(0), nullptr, nullptr);
    auto whole_tmp = new tree::SeqStm(
        left_cjump,
        new tree::SeqStm(new tree::LabelStm(mid_label), right_cjump));
    result_exp = new tr::CxExp(
        tr::PatchList(std::list<temp::Label **>{&(right_cjump->true_label_)}),
        tr::PatchList(std::list<temp::Label **>{&(left_cjump->false_label_),
                                                &(right_cjump->false_label_)}),
        whole_tmp);
    break;
  }
  // 同上逻辑
  case absyn::OR_OP: {
    auto mid_label = temp::LabelFactory::NewLabel();
    auto left_cjump =
        new tree::CjumpStm(tree::RelOp::NE_OP, left_tree->exp_->UnEx(),
                           new tree::ConstExp(0), nullptr, mid_label);
    auto right_cjump =
        new tree::CjumpStm(tree::RelOp::NE_OP, right_tree->exp_->UnEx(),
                           new tree::ConstExp(0), nullptr, nullptr);
    auto whole_tmp = new tree::SeqStm(
        left_cjump,
        new tree::SeqStm(new tree::LabelStm(mid_label), right_cjump));
    result_exp = new tr::CxExp(
        tr::PatchList(std::list<temp::Label **>{&(right_cjump->true_label_),
                                                &(left_cjump->true_label_)}),
        tr::PatchList(std::list<temp::Label **>{&(right_cjump->false_label_)}),
        whole_tmp);
    break;
  }
  }

  return new tr::ExpAndTy(result_exp, type::IntTy::Instance());
}

// record值语句
// 先分配全部的空间（field个数*8），然后拿着返回的指针对一个个filed进行初始化赋值
tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto record_temp = temp::TempFactory::NewTemp();
  size_t fields_sizes = this->fields_->GetList().size();
  // 将alloc_record分配后返回的指针付赋值给record的temp变量
  auto alloc_stm = new tree::MoveStm(
      new tree::TempExp(record_temp),
      new tree::CallExp(
          new tree::NameExp(temp::LabelFactory::NamedLabel("alloc_record")),
          new tree::ExpList{
              new tree::ConstExp(fields_sizes * reg_manager->WordSize())}));
  tree::Stm *init_stm = nullptr;
  auto efield_it = this->fields_->GetList().rbegin();
  auto efield_end = this->fields_->GetList().rend();
  if (efield_it != efield_end) {
    // 如果只有一个field,则不需要包裹一层SeqStm,反之则需要
    auto tran_efield =
        (*efield_it)->exp_->Translate(venv, tenv, level, label, errormsg);
    init_stm =
        tr::InitializeRecode(record_temp, tran_efield->exp_, --fields_sizes);
    ++efield_it;
    for (; efield_it != efield_end; ++efield_it) {
      tran_efield =
          (*efield_it)->exp_->Translate(venv, tenv, level, label, errormsg);
      init_stm = new tree::SeqStm(
          tr::InitializeRecode(record_temp, tran_efield->exp_, --fields_sizes),
          init_stm);
      ;
    }
  }
  // 使用EseqExp,以便返回record指针的Temp
  return new tr::ExpAndTy(
      new tr::ExExp(new tree::EseqExp(new tree::SeqStm(alloc_stm, init_stm),
                                      new tree::TempExp(record_temp))),
      tenv->Look(this->typ_));
}

// 语句列表
tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto exp_it = this->seq_->GetList().begin();
  auto exp_end = this->seq_->GetList().end();
  // 序列中语句列表为空
  if (exp_it == exp_end) {
    return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
  }
  auto exp_tran = (*exp_it)->Translate(venv, tenv, level, label, errormsg);
  if (++exp_it == exp_end) {
    // 只有一条语句，可以直接返回该语句的执行结果
    return exp_tran;
  } else {
    // 多条语句，前面的语句均不取其返回值，最后一条语句取值，作为整个语句序列的值
    auto seq_stm = exp_tran->exp_->UnNx();
    for (; std::next(exp_it) != exp_end; ++exp_it) {
      seq_stm = new tree::SeqStm(
          seq_stm, (*exp_it)
                       ->Translate(venv, tenv, level, label, errormsg)
                       ->exp_->UnNx());
    }
    // 最后一条语句，取值UnEx
    auto tran_seq_end =
        (*exp_it)->Translate(venv, tenv, level, label, errormsg);
    return new tr::ExpAndTy(
        new tr::ExExp(new tree::EseqExp(seq_stm, tran_seq_end->exp_->UnEx())),
        tran_seq_end->ty_);
  }
}

// 赋值语句
tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto tran_var = this->var_->Translate(venv, tenv, level, label, errormsg);
  auto tran_exp = this->exp_->Translate(venv, tenv, level, label, errormsg);
  return new tr::ExpAndTy(new tr::NxExp(new tree::MoveStm(
                              tran_var->exp_->UnEx(), tran_exp->exp_->UnEx())),
                          type::VoidTy::Instance());
}

// if语句
tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // test与then语句翻译（else不一定有，后续根据存在性选择是否翻译）
  auto tran_test = this->test_->Translate(venv, tenv, level, label, errormsg);
  auto tran_then = this->then_->Translate(venv, tenv, level, label, errormsg);
  // 判定true false的跳转标签，以及最后的joint标签
  auto true_label = temp::LabelFactory::NewLabel();
  auto false_label = temp::LabelFactory::NewLabel();
  auto joint_label = temp::LabelFactory::NewLabel();
  auto jump_cx = tran_test->exp_->UnCx(errormsg);
  // 将test建立的条件跳转语句的true和false标签赋入
  jump_cx.trues_.DoPatch(true_label);
  jump_cx.falses_.DoPatch(false_label);
  // if语句可能存在返回值，这是对应的取值标签
  auto result_temp = temp::TempFactory::NewTemp();
  if (this->elsee_) {
    // else语句存在的情况下，if语句存在返回值
    // 故执行完else或then后，需要将其执行的值移动进入result_temp,然后跳转至最后
    auto tran_else =
        this->elsee_->Translate(venv, tenv, level, label, errormsg);
    auto seq_stm = new tree::SeqStm(
        jump_cx.stm_,
        new tree::SeqStm(
            new tree::LabelStm(true_label),
            new tree::SeqStm(
                new tree::MoveStm(new tree::TempExp(result_temp),
                                  tran_then->exp_->UnEx()),
                new tree::SeqStm(
                    new tree::JumpStm(
                        new tree::NameExp(joint_label),
                        new std::vector<temp::Label *>{joint_label}),
                    new tree::SeqStm(
                        new tree::LabelStm(false_label),
                        new tree::SeqStm(
                            new tree::MoveStm(new tree::TempExp(result_temp),
                                              tran_else->exp_->UnEx()),
                            new tree::SeqStm(
                                new tree::JumpStm(
                                    new tree::NameExp(joint_label),
                                    new std::vector<temp::Label *>{
                                        joint_label}),
                                new tree::LabelStm(joint_label))))))));

    return new tr::ExpAndTy(new tr::ExExp(new tree::EseqExp(
                                seq_stm, new tree::TempExp(result_temp))),
                            tran_then->ty_);
  } else {
    // else语句不存在的情况下，if语句是无值的
    auto seq_stm = new tree::SeqStm(
        jump_cx.stm_,
        new tree::SeqStm(new tree::LabelStm(true_label),
                         new tree::SeqStm(tran_then->exp_->UnNx(),
                                          new tree::LabelStm(false_label))));
    delete result_temp;
    delete joint_label;
    return new tr::ExpAndTy(new tr::NxExp(seq_stm), type::VoidTy::Instance());
  }
}

// while语句，无返回值
tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto test_label = temp::LabelFactory::NewLabel();
  auto body_label = temp::LabelFactory::NewLabel();
  auto done_label = temp::LabelFactory::NewLabel();
  auto tran_test = this->test_->Translate(venv, tenv, level, label, errormsg);
  // 这里传入了done_label,方便内层break进行跳转
  auto tran_body =
      this->body_->Translate(venv, tenv, level, done_label, errormsg);
  auto jump_cx = tran_test->exp_->UnCx(errormsg);
  // test通过：进入body，后续v会再次跳入test;test不通过：跳入done
  jump_cx.trues_.DoPatch(body_label);
  jump_cx.falses_.DoPatch(done_label);
  auto result_stm = new tree::SeqStm(
      new tree::LabelStm(test_label),
      new tree::SeqStm(
          jump_cx.stm_,
          new tree::SeqStm(
              new tree::LabelStm(body_label),
              new tree::SeqStm(
                  tran_body->exp_->UnNx(),
                  new tree::SeqStm(
                      new tree::JumpStm(
                          new tree::NameExp(test_label),
                          new std::vector<temp::Label *>({test_label})),
                      new tree::LabelStm(done_label))))));

  return new tr::ExpAndTy(new tr::NxExp(result_stm), type::VoidTy::Instance());
}

// for循环语句，无返回值，会创建一个局部域的循环变量
tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  venv->BeginScope();
  // 在栈帧上分配新的变量，escape已经在逃逸分析中确定
  auto iterator_access = tr::Access::AllocLocal(level, this->escape_);
  auto tran_lo = this->lo_->Translate(venv, tenv, level, label, errormsg);
  auto tran_hi = this->hi_->Translate(venv, tenv, level, label, errormsg);
  auto test_label = temp::LabelFactory::NewLabel();
  auto body_label = temp::LabelFactory::NewLabel();
  auto done_label = temp::LabelFactory::NewLabel();
  venv->Enter(this->var_, new env::VarEntry(iterator_access, tran_lo->ty_));
  // body翻译
  auto tran_body =
      this->body_->Translate(venv, tenv, level, done_label, errormsg);
  // 获取循环变量的语句，帧指针就是当前帧
  auto iterator_exp = iterator_access->access_->getT_EXP(
      new tree::TempExp(reg_manager->FramePointer()));
  auto result_stm = new tree::SeqStm(
      // 赋初值
      new tree::MoveStm(iterator_exp, tran_lo->exp_->UnEx()),
      // test检验
      new tree::SeqStm(
          new tree::LabelStm(test_label),

          new tree::SeqStm(
              new tree::CjumpStm(tree::RelOp::LE_OP, iterator_exp,
                                 tran_hi->exp_->UnEx(), body_label, done_label),
              // 循环体
              new tree::SeqStm(
                  new tree::LabelStm(body_label),

                  new tree::SeqStm(
                      tran_body->exp_->UnNx(),
                      // 循环变量+1,赋值给自身
                      new tree::SeqStm(
                          new tree::MoveStm(
                              iterator_exp,
                              new tree::BinopExp(tree::BinOp::PLUS_OP,
                                                 iterator_exp,
                                                 new tree::ConstExp(1))),
                          // 更新循环变量值后，重新进入test语句检验
                          new tree::SeqStm(
                              new tree::JumpStm(
                                  new tree::NameExp(test_label),
                                  new std::vector<temp::Label *>({test_label})),
                              new tree::LabelStm(done_label))))))));
  venv->EndScope();
  return new tr::ExpAndTy(new tr::NxExp(result_stm), type::VoidTy::Instance());
}

// break语句，直接根据传入的循环done label进行跳转即可
tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(
      new tr::NxExp(new tree::JumpStm(new tree::NameExp(label),
                                      new std::vector<temp::Label *>({label}))),
      type::VoidTy::Instance());
}

// let in语句
tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  venv->BeginScope();
  tenv->BeginScope();
  tree::Stm *dec_stm = nullptr;
  tr::ExpAndTy *result = nullptr;

  // 同之前的策略，如果只有一个类型声明语句，dec_stm不需要包裹tree::SeqStm，反之则需要
  auto dec_list = this->decs_->GetList();
  for (auto dec_item : dec_list) {
    if (dec_stm == nullptr) {
      dec_stm = dec_item->Translate(venv, tenv, level, label, errormsg)->UnNx();
    } else {
      dec_stm = new tree::SeqStm(
          dec_stm,
          dec_item->Translate(venv, tenv, level, label, errormsg)->UnNx());
    }
  }
  // body如果为空，则返回空的语句，否则需要进行翻译
  tr::ExpAndTy *tran_body;
  if (this->body_ == nullptr) {
    tran_body = new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                                 type::VoidTy::Instance());
  } else {
    tran_body = this->body_->Translate(venv, tenv, level, label, errormsg);
  }

  // 如果没有声明语句，则dec_stm为nullptr,此时不需要EseqExp，否则需要
  if (dec_stm == nullptr) {
    result = tran_body;
  } else {
    result = new tr::ExpAndTy(
        new tr::ExExp(new tree::EseqExp(dec_stm, tran_body->exp_->UnEx())),
        tran_body->ty_);
  }

  tenv->EndScope();
  venv->EndScope();
  return result;
}

// array变量初始化语句，需要调用外部函数“init_array”
tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto array_type = tenv->Look(this->typ_)->ActualTy();
  auto tran_size = this->size_->Translate(venv, tenv, level, label, errormsg);
  auto tran_init = this->init_->Translate(venv, tenv, level, label, errormsg);
  auto exp_list = new tree::ExpList();
  exp_list->Append(tran_size->exp_->UnEx());
  exp_list->Append(tran_init->exp_->UnEx());
  return new tr::ExpAndTy(
      new tr::ExExp(new tree::CallExp(
          new tree::NameExp(temp::LabelFactory::NamedLabel("init_array")),
          exp_list)),
      array_type);
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(
      new tr::NxExp(new tree::ExpStm(new tree::ConstExp(0))),
      type::VoidTy::Instance());
}

// 函数定义语句，多个函数定义，需要扫描两遍
tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // 获取函数定义表并开始遍历
  auto fun_dec_list = this->functions_->GetList();
  for (auto fun_dec_item : fun_dec_list) {
    // 获取参数表
    auto param_list = fun_dec_item->params_->GetList();
    // 根据逃逸分析情况构建逃逸表
    auto escape_list = new std::list<bool>;
    for (auto param_item : param_list) {
      escape_list->push_back(param_item->escape_);
    }
    // 为函数申请新的跳转标签，以其名字为标识
    auto fun_label =
        temp::LabelFactory::NamedLabel(fun_dec_item->name_->Name());
    // 构建新的level,包含其初始化栈帧（带有进入函数时的所有传入参数，其已经根据逃逸情况在栈帧上完成分配）
    auto fun_level = new tr::Level(level, fun_label, escape_list);
    auto param_ty_list =
        fun_dec_item->params_->MakeFormalTyList(tenv, errormsg);
    type::Ty *return_type =
        (fun_dec_item->result_ == nullptr)
            ? (type::VoidTy::Instance())
            : (tenv->Look(fun_dec_item->result_)->ActualTy());
    // venv加入函数定义
    venv->Enter(
        fun_dec_item->name_,
        new env::FunEntry(fun_level, fun_label, param_ty_list, return_type));
    delete escape_list;
  }

  // 第二遍，扫描函数头和函数体
  for (auto fun_dec_item : fun_dec_list) {
    env::FunEntry *fun_entry =
        static_cast<env::FunEntry *>(venv->Look(fun_dec_item->name_));
    auto param_it = fun_dec_item->params_->GetList().begin();
    auto param_end = fun_dec_item->params_->GetList().end();
    auto frame_access_it = fun_entry->level_->frame_->formal_list->begin();
    auto param_type_it = fun_entry->formals_->GetList().begin();
    ++frame_access_it;
    venv->BeginScope();
    // 传入参数加入venv
    for (; param_it != param_end;
         ++param_it, ++frame_access_it, ++param_type_it) {
      venv->Enter(
          (*param_it)->name_,
          new env::VarEntry(new tr::Access(fun_entry->level_, *frame_access_it),
                            (*param_type_it)));
    }
    auto tran_body = fun_dec_item->body_->Translate(
        venv, tenv, fun_entry->level_, fun_entry->label_, errormsg);
    // 将body值移动进入指定寄存器
    auto return_stm = new tree::MoveStm(
        new tree::TempExp(reg_manager->ReturnValue()), tran_body->exp_->UnEx());
    // frags加入新的函数片段
    frags->PushBack(
        frame::ProcEntryExit1(fun_entry->level_->frame_, return_stm));
    venv->EndScope();
  }
  return new tr::ExExp(new tree::ConstExp(0));
}

// 变量定义语句
tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // 栈帧上分配内容
  auto var_access = tr::Access::AllocLocal(level, this->escape_);
  // 初始值翻译
  tr::ExpAndTy *tran_init =
      this->init_->Translate(venv, tenv, level, label, errormsg);
  // venv加入
  venv->Enter(this->var_, new env::VarEntry(var_access, tran_init->ty_));
  // 将初始化值移入栈帧上为该变量分配的位置
  return new tr::NxExp(
      new tree::MoveStm(var_access->access_->getT_EXP(
                            new tree::TempExp(reg_manager->FramePointer())),
                        tran_init->exp_->UnEx()));
}

// 类型定义语句，扫描两遍
tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto type_dec_list = this->types_->GetList();
  for (auto type_dec_item : type_dec_list) {
    tenv->Enter(type_dec_item->name_,
                new type::NameTy(type_dec_item->name_, nullptr));
  }
  for (auto type_dec_item : type_dec_list) {
    auto name_ty =
        static_cast<type::NameTy *>(tenv->Look(type_dec_item->name_));
    name_ty->ty_ = type_dec_item->ty_->Translate(tenv, errormsg);
  }
  return new tr::ExExp(new tree::ConstExp(0));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto name_ty_ty = tenv->Look(this->name_);
  return new type::NameTy(this->name_, name_ty_ty);
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new type::RecordTy(this->record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new type::ArrayTy(tenv->Look(this->array_)->ActualTy());
}

} // namespace absyn
