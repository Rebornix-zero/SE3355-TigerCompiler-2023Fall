#include "tiger/semant/semant.h"
#include "tiger/absyn/absyn.h"

#define TYPECHECK(type1, type2) ((typeid(type1)) == (typeid(type2)))

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  root_->SemAnalyze(venv, tenv, 0, errormsg);
}

// 通过SimpleVar中的symbol变量在venv中查找SimpleVar的实际类型，如果找到则返回，找不到则报错
type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *var_entry = venv->Look(this->sym_);
  if (var_entry != nullptr && TYPECHECK(*var_entry, env::VarEntry)) {
    return (static_cast<env::VarEntry *>(var_entry))->ty_->ActualTy();
  } else {
    std::string error = "undefined variable " + this->sym_->Name();
    errormsg->Error(this->pos_, error);
    return type::IntTy::Instance();
  }
}

// FieldVar包含一个变量var,一个symbol sym_，其代表了一个record下的一个field
// 先将var进行类型分析获得其对应的类型
// 如果不是record，则将无法使用sym,是错误的
// 如果是record,则遍历此record类型的fieldlist,找到symbol匹配的部分，返回其Actually值
type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *record_type =
      this->var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (record_type == nullptr || !(TYPECHECK(*record_type, type::RecordTy))) {
    std::string error = "not a record type";
    errormsg->Error(this->pos_, error);
    return type::VoidTy::Instance();
  }
  std::list<type::Field *> fieldlist =
      (static_cast<type::RecordTy *>(record_type))->fields_->GetList();
  for (type::Field *item : fieldlist) {
    if (item->name_ == this->sym_) {
      return item->ty_->ActualTy();
    }
  }
  std::string error = "field " + this->sym_->Name() + " doesn't exist";
  errormsg->Error(this->pos_, error);
  return type::IntTy::Instance();
}

// SubscriptVar指数组变量的一个元素
// 分析同上，如果var不是数组类型则报错
// 如果是，则返回这个数组类型的元素类型（不需要匹配，因为数组元素的类型是一样的）
type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *array_type = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (array_type != nullptr && TYPECHECK(*array_type, type::ArrayTy)) {
    return (static_cast<type::ArrayTy *>(array_type))->ty_;
  } else {
    std::string error = "array type required";
    errormsg->Error(this->pos_, error);
    return type::IntTy::Instance();
  }
}

// 有值的变量，对其中的变量做类型检查后，返回其类型即可
type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return this->var_->SemAnalyze(venv, tenv, labelcount, errormsg);
}

// 空值语句，返回Nil类型
type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::NilTy::Instance();
}

// Int语句，返回Int类型
type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::IntTy::Instance();
}

// String语句，返回String类型
type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::StringTy::Instance();
}

// 函数调用语句，包含调用的函数符号名和其对应的参数列表
type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // 先检查symbol是否是一个真正的函数符号
  env::EnvEntry *fun_entry = venv->Look(this->func_);
  if (fun_entry == nullptr) {
    std::string error = "undefined function " + this->func_->Name();
    errormsg->Error(this->pos_, error);
    return type::VoidTy::Instance();
  }
  if (!TYPECHECK(*fun_entry, env::FunEntry)) {
    std::string error = "need to be a function";
    errormsg->Error(this->pos_, error);
    return type::VoidTy::Instance();
  }
  // 再检查参数列表中的类型是否一一匹配，数量是否一致，二者出现任一错误都不允许
  std::list<type::Ty *> formals_list =
      (static_cast<env::FunEntry *>(fun_entry))->formals_->GetList();
  type::Ty *result_ty = (static_cast<env::FunEntry *>(fun_entry))->result_;
  const std::list<absyn::Exp *> argu_list = this->args_->GetList();

  std::list<type::Ty *>::iterator fit = formals_list.begin();
  std::list<type::Ty *>::iterator fend = formals_list.end();
  std::list<absyn::Exp *>::const_iterator ait = argu_list.begin();
  std::list<absyn::Exp *>::const_iterator aend = argu_list.end();

  for (; fit != fend && ait != aend; ++fit, ++ait) {
    // 每个参数应该先进行类型检查，获取其类型，在与函数定义的此处参数类型进行比较
    type::Ty *single_argu =
        (*ait)->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!single_argu->IsSameType(*fit)) {
      std::string error = "para type mismatch";
      errormsg->Error(this->pos_, error);
      return type::VoidTy::Instance();
    }
  }
  // 如果长度不匹配，f或a会有参数的剩余；否则其应该同时递归至end结束
  if (fit != fend) {
    std::string error = "para type mismatch";
    errormsg->Error(this->pos_, error);
    return type::VoidTy::Instance();
  }
  if (ait != aend) {
    std::string error = "too many params in function " + this->func_->Name();
    errormsg->Error(this->pos_, error);
  }
  return result_ty;
}

// 二元运算语句，先对两边exp做类型检查，然后查看其类型是否匹配二元运算需要的类型即可
type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *left_ty = this->left_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *right_ty =
      this->right_->SemAnalyze(venv, tenv, labelcount, errormsg);

  absyn::Oper op = this->oper_;
  if (op == PLUS_OP || op == MINUS_OP || op == TIMES_OP || op == DIVIDE_OP) {
    if (!TYPECHECK(*left_ty, type::IntTy)) {
      std::string error = "integer required";
      errormsg->Error(this->left_->pos_, error);
    }
    if (!TYPECHECK(*right_ty, type::IntTy)) {
      std::string error = "integer required";
      errormsg->Error(this->right_->pos_, error);
    }
    return type::IntTy::Instance();
  } else {
    if (!left_ty->IsSameType(right_ty)) {
      std::string error = "same type required";
      errormsg->Error(this->pos_, error);
    }
    return type::IntTy::Instance();
  }
}

// record值语句，symbol是其对应的record
// type的symbol,efieldlist是其中field示例的list
type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // 检查这个record的类型symbol是否真的对应了一个record类型
  type::Ty *record_ty = tenv->Look(this->typ_);
  if (record_ty == nullptr) {
    std::string error = "undefined type " + this->typ_->Name();
    errormsg->Error(this->pos_, error);
    return type::VoidTy::Instance();
  }
  record_ty = record_ty->ActualTy();
  if (!TYPECHECK(*record_ty, type::RecordTy)) {
    errormsg->Error(this->pos_, "not a record type");
    return type::IntTy::Instance();
  }
  const std::list<absyn::EField *> efield_list = this->fields_->GetList();
  std::list<type::Field *> field_list =
      (static_cast<type::RecordTy *>(record_ty))->fields_->GetList();

  auto efit = efield_list.begin();
  auto efend = efield_list.end();
  auto fit = field_list.begin();
  auto fend = field_list.end();

  // 先解析每个efield,然后与对应的record的field的type进行匹配检验，
  // 两者任何一方提前停止匹配，都表示数量不符合，从而报错
  for (; efit != efend && fit != fend; ++efit, ++fit) {
    if ((*efit)->name_ != (*fit)->name_) {
      std::string error = "field type mismatch";
      errormsg->Error(this->pos_, error);
      return type::VoidTy::Instance();
    }
    type::Ty *single_efield =
        (*efit)->exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!single_efield->IsSameType((*fit)->ty_)) {
      std::string error = "field and efield type mismatch";
      errormsg->Error(this->pos_, error);
      return type::VoidTy::Instance();
    }
  }
  if (efit != efend || fit != fend) {
    std::string error = "field type mismatch";
    errormsg->Error(this->pos_, error);
    return type::VoidTy::Instance();
  }
  return record_ty;
}

// 对其中的exp序列逐个类型检查,返回最后一个exp的type
type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */

  std::list<Exp *> exp_list = this->seq_->GetList();
  auto el_it = exp_list.begin();
  auto el_end = exp_list.end();
  type::Ty *result = nullptr;
  for (; el_end != el_it; ++el_it) {
    result = (*el_it)->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  return result;
}

// 赋值语句
// 检查赋值变量和值的类型是否一致，如果一致则允许，否则则拒绝
// 注意，如循环变量等变量是不可被赋值语句赋值的，可以通过readonly标记发现这样的变量，并报错
type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *var_type =
      this->var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (TYPECHECK(*(this->var_), absyn::SimpleVar)) {
    env::EnvEntry *var_entry =
        venv->Look((static_cast<SimpleVar *>(this->var_)->sym_));
    if ((static_cast<env::VarEntry *>(var_entry))->readonly_) {
      errormsg->Error(this->pos_, "loop variable can't be assigned");
      return type::VoidTy::Instance();
    }
  }
  type::Ty *exp_type = this->exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!(var_type->IsSameType(exp_type))) {
    errormsg->Error(this->pos_, "unmatched assign exp");
  }
  return type::VoidTy::Instance();
}

// if语句
// 分别对if then else三个语句做类型检查即可
// 注意，单独then语句的时候，返回类型应该是Void
// 而then和else语句同时存在时，允许一个非void的返回类型，但是then和else语句内容应该相等
type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *test_type =
      this->test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (!TYPECHECK(*test_type, type::IntTy)) {
    errormsg->Error(this->pos_, "test exp of If needs to be IntTy");
  }
  type::Ty *then_type =
      this->then_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (this->elsee_ != nullptr) {
    type::Ty *else_type =
        this->elsee_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
    if (!(then_type->IsSameType(else_type))) {
      errormsg->Error(this->pos_, "then exp and else exp type mismatch");
    }
    return then_type;
  } else {
    if (!TYPECHECK(*then_type, type::VoidTy)) {
      errormsg->Error(this->pos_, "if-then exp's body must produce no value");
    }
    return type::VoidTy::Instance();
  }
}

// while语句，函数体只能返回VoidTy
type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *test_type =
      this->test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (!TYPECHECK(*test_type, type::IntTy)) {
    errormsg->Error(this->pos_, "test exp of while needs to be IntTy");
  }
  type::Ty *body_type =
      this->body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg)->ActualTy();
  if (!TYPECHECK(*body_type, type::VoidTy)) {
    errormsg->Error(this->pos_, "while body must produce no value");
  }
  return type::VoidTy::Instance();
}

// for循环语句
// 在这个循环中定义了新的只读循环变量，在离开这个循环后此变量不可用，
// 故需要BeginScope和EndScope
// 函数体只能返回VoidTy
type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  venv->BeginScope();
  type::Ty *lo_type =
      this->lo_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *hi_type =
      this->hi_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (!TYPECHECK(*lo_type, type::IntTy) || !TYPECHECK(*hi_type, type::IntTy)) {
    errormsg->Error(this->pos_, "for exp's range type is not integer");
  }
  venv->Enter(this->var_, new env::VarEntry(type::IntTy::Instance(), true));
  type::Ty *body_type =
      this->body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg)->ActualTy();
  if (!TYPECHECK(*body_type, type::VoidTy)) {
    errormsg->Error(this->pos_, "for's body must be a void exp");
  }
  venv->EndScope();
  return type::VoidTy::Instance();
}

// labelcount=0则代表在非循环体中调用了break,这是不允许的
type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  if (labelcount == 0) {
    errormsg->Error(this->pos_, "break is not inside any loop");
  }
  return type::VoidTy::Instance();
}

// let in语句
type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *result = type::VoidTy::Instance();
  venv->BeginScope();
  tenv->BeginScope();
  const std::list<Dec *> dec_list = this->decs_->GetList();
  for (Dec *item : dec_list) {
    item->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  if (this->body_ != nullptr) {
    result =
        this->body_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  }
  venv->EndScope();
  tenv->EndScope();
  return result;
}

// Array赋值语句，指定了array的类型，数组大小和数组的初始赋值
type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // 检查symbol是否为数组类型
  type::Ty *array_type = tenv->Look(this->typ_)->ActualTy();

  if (array_type == nullptr || !TYPECHECK(*array_type, type::ArrayTy)) {
    std::string error = "not array type " + this->typ_->Name();
    errormsg->Error(this->pos_, error);
  }

  // 获取array type的元素类型
  type::Ty *array_item_type =
      (static_cast<type::ArrayTy *>(array_type))->ty_->ActualTy();
  // 检查size和init的值类型是否匹配
  type::Ty *size_type =
      this->size_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *init_type =
      this->init_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (!TYPECHECK(*size_type, type::IntTy)) {
    errormsg->Error(this->pos_, "size of array must be int");
  }
  if (!(init_type->IsSameType(array_item_type))) {
    errormsg->Error(this->pos_, "type mismatch");
  }
  return array_type;
}

// Void语句，返回Void类型
type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::VoidTy::Instance();
}

// 函数定义语句，其中可能包含多个函数定义
// 需要扫描两次，以避免相互调用时查找不到：
// 第一次：只扫描函数头，将这个函数的定义加入venv
// 第二次：扫描函数头和函数体，真正为其做类型检查
void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  const std::list<FunDec *> fun_dec_list = this->functions_->GetList();
  for (FunDec *item : fun_dec_list) {
    // 获取函数符号，参数列表，返回值
    sym::Symbol *fun_name = item->name_;
    type::TyList *formal_lsit = item->params_->MakeFormalTyList(tenv, errormsg);
    type::Ty *return_type;
    // 检查返回值类型
    if (item->result_ != nullptr) {
      return_type = tenv->Look(item->result_);
      if (return_type == nullptr) {
        // 查找不到返回类型
        return_type = type::VoidTy::Instance();
      }
    } else {
      return_type = type::VoidTy::Instance();
    }
    // 不允许两个函数重名
    if (venv->Look(fun_name) != nullptr) {
      errormsg->Error(this->pos_, "two functions have the same name");
      return;
    }
    venv->Enter(fun_name, new env::FunEntry(formal_lsit, return_type));
  }

  for (FunDec *item : fun_dec_list) {
    env::FunEntry *fun_entry =
        static_cast<env::FunEntry *>(venv->Look(item->name_));
    std::list<absyn::Field *> field_list = item->params_->GetList();
    auto field_it = field_list.begin();
    auto field_end = field_list.end();
    auto formal_it = fun_entry->formals_->GetList().begin();
    venv->BeginScope();
    for (; field_it != field_end; ++field_it, ++formal_it) {
      venv->Enter((*field_it)->name_, new env::VarEntry(*formal_it));
    }
    type::Ty *execute_type =
        item->body_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
    if (!fun_entry->result_->IsSameType(execute_type)) {
      if (TYPECHECK(*(fun_entry->result_), type::VoidTy)) {
        errormsg->Error(this->pos_, "procedure returns value");
      } else {
        errormsg->Error(this->pos_, "return type mismatch");
      }
    }
    venv->EndScope();
  }
}

// 变量声明语句：将符号的类型加入venv表中
// 注意：初始值类型需要与变量的类型一致，或为nil
// 但是未指定类型的变量初始值不能为nil
void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *init_type =
      this->init_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (this->typ_ != nullptr) {
    type::Ty *look_type = tenv->Look(this->typ_);
    if (look_type == nullptr || !look_type->IsSameType(init_type)) {
      errormsg->Error(pos_, "type mismatch");
    }
  } else {
    if (TYPECHECK((*init_type), type::NilTy)) {
      errormsg->Error(pos_, "init should not be nil without type specified");
    }
  }
  venv->Enter(this->var_, new env::VarEntry(init_type));
}

// 类型声明语句，其中可能包含多个类型定义
// 需要扫描两次：
// 第一次：将类型先加入表中
// 第二次：实际进行类型解析
// 避免类型的互相循环定义，需要进行检查
void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  const std::list<absyn::NameAndTy *> type_list = this->types_->GetList();
  // 先以NameType方式声明所有type
  for (absyn::NameAndTy *item : type_list) {
    if (tenv->Look(item->name_) != nullptr) {
      errormsg->Error(this->pos_, "two types have the same name");
      return;
    }
    tenv->Enter(item->name_, new type::NameTy(item->name_, nullptr));
  }
  // 再对所有的type进行实际的类型声明解析
  for (absyn::NameAndTy *item : type_list) {
    type::NameTy *item_name_type =
        static_cast<type::NameTy *>(tenv->Look(item->name_));
    item_name_type->ty_ = item->ty_->SemAnalyze(tenv, errormsg);
  }

  // check cycle cite
  for (absyn::NameAndTy *item : type_list) {
    bool cycle = false;
    sym::Symbol *first_name = item->name_;
    type::Ty *type_current = tenv->Look(first_name);
    while (true) {
      type::Ty *next_type = (static_cast<type::NameTy *>(type_current))->ty_;
      if (!TYPECHECK((*next_type), type::NameTy)) {
        break;
      } else {
        if ((static_cast<type::NameTy *>(next_type))->sym_ == first_name) {
          // 如果绕了一圈都是NameTy,没有指向实际的类型，则绕一圈后即证明这是循环类型声明，退出检查并准备报错
          cycle = true;
          break;
        }
        type_current = next_type;
      }
    }
    if (cycle) {
      errormsg->Error(this->pos_, "illegal type cycle");
      break;
    }
  }
}

// absyn::NameTy指定了一个类型名，通过其获取对应的类型
type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *name_type = tenv->Look(this->name_);
  if (name_type == nullptr) {
    std::string error = "undefined type " + this->name_->Name();
    errormsg->Error(this->pos_, error);
    return type::VoidTy::Instance();
  }
  return new type::NameTy(this->name_, name_type);
}

// 记录类型声明语句，将记录的类型返回即可
type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return new type::RecordTy(this->record_->MakeFieldList(tenv, errormsg));
}

// 数组类型声明语句，将记录的数组类型返回即可
// 需要先检查其元素的类型是否存在，当元素类型已经声明后，这个数组类型才是有意义的
type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *array_type = tenv->Look(this->array_)->ActualTy();
  if (array_type == nullptr) {
    std::string error = "undefined type " + this->array_->Name();
    errormsg->Error(this->pos_, error);
    return type::VoidTy::Instance();
  }
  return new type::ArrayTy(array_type);
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}

} // namespace sem
