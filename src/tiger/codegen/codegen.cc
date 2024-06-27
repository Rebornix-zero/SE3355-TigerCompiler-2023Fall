#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;

} // namespace

namespace cg {

void CodeGen::Codegen() {
  /* TODO: Put your lab5 code here */
  auto list = new assem::InstrList();
  // callee save寄存器保存
  std::deque<temp::Temp *>
      callee_reg_list; // 记录了用作callee save的临时变量列表
  auto rm_callee_list = reg_manager->CalleeSaves()->GetList();
  for (auto callee_item : rm_callee_list) {
    auto new_temp = temp::TempFactory::NewTemp();
    callee_reg_list.push_back(new_temp);
    list->Append(new assem::MoveInstr("movq `s0, `d0",
                                      new temp::TempList(new_temp),
                                      new temp::TempList(callee_item)));
  }
  auto trace_list = this->traces_->GetStmList()->GetList();
  for (auto trace_item : trace_list) {
    trace_item->Munch(*list, this->fs_);
  }
  // callee save寄存器恢复
  int current_reg_num = 0;
  for (auto callee_item : rm_callee_list) {
    list->Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList(callee_item),
        new temp::TempList(callee_reg_list.at(current_reg_num))));
    ++current_reg_num;
  }
  this->assem_instr_ =
      std::make_unique<AssemInstr>(frame::ProcEntryExit2(list));
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {
/* TODO: Put your lab5 code here */

// SeqStm：左右两个Stm进行指令选择即可
void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  this->left_->Munch(instr_list, fs);
  this->right_->Munch(instr_list, fs);
}

// LabelStm：创建标签
void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::LabelInstr(this->label_->Name(), this->label_));
}

// 无条件跳转语句
void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::OperInstr("jmp `j0", new temp::TempList(),
                                         new temp::TempList(),
                                         new assem::Targets(this->jumps_)));
}

// 跳转跳转语句
// 注意tree::CjumpStm是左式比较右式，而X64的cmpq反过来
// 实现方式是左右式不变（保持树的执行顺序不变），而Relop对应的条件跳转符号对调
void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::OperInstr(
      "cmpq `s0, `s1", new temp::TempList(),
      new temp::TempList({this->left_->Munch(instr_list, fs),
                          this->right_->Munch(instr_list, fs)}),
      nullptr));
  std::string jump_str = "";
  switch (this->op_) {
  case tree::RelOp::EQ_OP: {
    jump_str += "je ";
    break;
  }
  case tree::RelOp::NE_OP: {
    jump_str += "jne ";
    break;
  }
  case tree::RelOp::LT_OP: {
    jump_str += "jg ";
    break;
  }
  case tree::RelOp::GT_OP: {
    jump_str += "jl ";
    break;
  }
  case tree::RelOp::LE_OP: {
    jump_str += "jge ";
    break;
  }
  case tree::RelOp::GE_OP: {
    jump_str += "jle ";
    break;
  }
  }
  jump_str += "`j0";
  instr_list.Append(new assem::OperInstr(
      jump_str, new temp::TempList(), new temp::TempList(),
      new assem::Targets(new std::vector<temp::Label *>({this->true_label_}))));
}

// 使用最大覆盖法考虑MoveStm的所有情况，优先以大瓦片进行覆盖
void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // movq s, Mem
  if (typeid(*dst_) == typeid(MemExp)) {
    auto dst2mem = static_cast<MemExp *>(dst_);
    if (typeid(*dst2mem->exp_) == typeid(BinopExp)) {
      // dst MEM BINOP
      auto mem2bin = static_cast<BinopExp *>(dst2mem->exp_);
      if (mem2bin->op_ == PLUS_OP) {
        // dst MEM + 可能存在更大的瓦片覆盖
        if (typeid(*mem2bin->left_) == typeid(ConstExp)) {
          // dst MEM + CONST(左)
          //  解析非常数部分的指令，解析完成后dst位置即解析成功
          auto right_temp = mem2bin->right_->Munch(instr_list, fs);
          auto left2cst = static_cast<ConstExp *>(mem2bin->left_);
          std::ostringstream assem;
          if (typeid(*src_) == typeid(ConstExp)) {
            // dst MEM + CONST(左) src CONST
            auto src2cst = static_cast<ConstExp *>(src_);
            // 非常数部分使用帧指针:需要转换为栈指针获取帧内容
            if (right_temp == reg_manager->FramePointer()) {
              assem << "movq $" << src2cst->consti_ << ", " << fs
                    << "_framesize" << ((left2cst->consti_ >= 0) ? "+" : "")
                    << left2cst->consti_ << "(`s0)";
              right_temp = reg_manager->StackPointer();
            } else {
              // 非常数部分不使用帧指针
              assem << "movq $" << src2cst->consti_ << "," << left2cst->consti_
                    << "(`s0)";
            }
            instr_list.Append(
                new assem::OperInstr(assem.str(), new temp::TempList(),
                                     new temp::TempList(right_temp), nullptr));
          } else {
            // dst MEM + CONST(左) src 非CONST
            if (right_temp == reg_manager->FramePointer()) {
              assem << "movq `s0, " << fs << "_framesize"
                    << ((left2cst->consti_ >= 0) ? "+" : "")
                    << left2cst->consti_ << "(`s1)";
              right_temp = reg_manager->StackPointer();
            } else {
              assem << "movq `s0," << left2cst->consti_ << "(`s1)";
            }
            instr_list.Append(new assem::OperInstr(
                assem.str(), new temp::TempList(),
                new temp::TempList{src_->Munch(instr_list, fs), right_temp},
                nullptr));
          }
        } else if (typeid(*mem2bin->right_) == typeid(ConstExp)) {
          // MEM + CONST(右)大部分内容同上
          auto left_temp = mem2bin->left_->Munch(instr_list, fs);
          auto right2cst = static_cast<ConstExp *>(mem2bin->right_);
          std::ostringstream assem;
          if (typeid(*src_) == typeid(ConstExp)) {
            // dst MEM + CONST(右) src CONST
            auto src2cst = static_cast<ConstExp *>(src_);
            if (left_temp == reg_manager->FramePointer()) {
              assem << "movq $" << src2cst->consti_ << ", " << fs
                    << "_framesize" << ((right2cst->consti_ >= 0) ? "+" : "")
                    << right2cst->consti_ << "(`s0)";
              left_temp = reg_manager->StackPointer();
            } else {
              assem << "movq $" << src2cst->consti_ << "," << right2cst->consti_
                    << "(`s0)";
            }
            instr_list.Append(
                new assem::OperInstr(assem.str(), new temp::TempList(),
                                     new temp::TempList(left_temp), nullptr));
          } else {
            // dst MEM + CONST(右) src 非CONST
            if (left_temp == reg_manager->FramePointer()) {
              assem << "movq `s0, " << fs << "_framesize"
                    << ((right2cst->consti_ >= 0) ? "+" : "")
                    << right2cst->consti_ << "(`s1)";
              left_temp = reg_manager->StackPointer();
            } else {
              assem << "movq `s0," << right2cst->consti_ << "(`s1)";
            }
            instr_list.Append(new assem::OperInstr(
                assem.str(), new temp::TempList(),
                new temp::TempList{src_->Munch(instr_list, fs), left_temp},
                nullptr));
          }
        } else {
          //+ 两侧均不是CONST
          instr_list.Append(new assem::OperInstr(
              "movq `s0,(`s1)", new temp::TempList(),
              new temp::TempList{src_->Munch(instr_list, fs),
                                 dst2mem->exp_->Munch(instr_list, fs)},
              nullptr));
        }
      }
    } else if (typeid(*dst2mem->exp_) == typeid(ConstExp)) {
      // dst MEM CONST
      //  X64未见这种使用方法
    } else {
      // dst MEM
      if (typeid(*src_) == typeid(ConstExp)) {
        // src CONST
        std::stringstream assem;
        auto src2cst = static_cast<ConstExp *>(src_);
        assem << "movq $" << src2cst->consti_ << ",(`s0)";
        instr_list.Append(new assem::OperInstr(
            assem.str(), new temp::TempList(),
            new temp::TempList(dst2mem->Munch(instr_list, fs)), nullptr));
      } else {
        // src 其他情况
        instr_list.Append(new assem::OperInstr(
            "movq `s0,(`s1)", new temp::TempList(),
            new temp::TempList{src_->Munch(instr_list, fs),
                               dst2mem->exp_->Munch(instr_list, fs)},
            nullptr));
      }
    }
  } else if (typeid(*src_) == typeid(MemExp)) {
    // dst非MEM 但src是MEM
    auto src2mem = static_cast<MemExp *>(src_);
    if (typeid(*src2mem->exp_) == typeid(BinopExp)) {
      // src MEM BINOP
      auto mem2bin = static_cast<BinopExp *>(src2mem->exp_);
      if (mem2bin->op_ == PLUS_OP) {
        if (typeid(*mem2bin->left_) == typeid(ConstExp)) {
          // movq Imm(r), d
          auto right_temp = mem2bin->right_->Munch(instr_list, fs);
          auto left2cst = static_cast<ConstExp *>(mem2bin->left_);
          std::ostringstream assem;
          if (right_temp == reg_manager->FramePointer()) {
            if (left2cst->consti_ < 0) {
              assem << "movq " << fs << "_framesize" << left2cst->consti_
                    << "(`s0),"
                    << "`d0";
            } else {
              assem << "movq " << fs << "_framesize+" << left2cst->consti_
                    << "(`s0),"
                    << "`d0";
            }
            right_temp == reg_manager->StackPointer();
          } else {
            assem << "movq " << left2cst->consti_ << "(`s0),"
                  << "`d0";
          }
          instr_list.Append(new assem::OperInstr(
              assem.str(), new temp::TempList(dst_->Munch(instr_list, fs)),
              new temp::TempList(right_temp), nullptr));
        } else if (typeid(*mem2bin->right_) == typeid(ConstExp)) {
          // movq Imm(r), d
          auto left_temp = mem2bin->left_->Munch(instr_list, fs);
          auto right2cst = static_cast<ConstExp *>(mem2bin->right_);
          std::ostringstream assem;
          if (left_temp == reg_manager->FramePointer()) {
            if (right2cst->consti_ < 0) {
              assem << "movq " << fs << "_framesize" << right2cst->consti_
                    << "(`s0),"
                    << "`d0";
            } else {
              assem << "movq (" << fs << "_framesize+" << right2cst->consti_
                    << ")(`s0),"
                    << "`d0";
            }
            left_temp = reg_manager->StackPointer();
          } else {
            assem << "movq " << right2cst->consti_ << "(`s0),"
                  << "`d0";
          }
          instr_list.Append(new assem::OperInstr(
              assem.str(), new temp::TempList(dst_->Munch(instr_list, fs)),
              new temp::TempList(left_temp), nullptr));
        } else {
          // movq (r), d
          instr_list.Append(new assem::OperInstr(
              "movq (`s0),`d0", new temp::TempList(dst_->Munch(instr_list, fs)),
              new temp::TempList(src2mem->exp_->Munch(instr_list, fs)),
              nullptr));
        }
      }
    } else if (typeid(*src2mem->exp_) == typeid(ConstExp)) {
      // src MEM CONST
      // X64未见此用法
    } else {
      // src MEM
      instr_list.Append(new assem::OperInstr(
          "movq (`s0),`d0", new temp::TempList(dst_->Munch(instr_list, fs)),
          new temp::TempList(src2mem->exp_->Munch(instr_list, fs)), nullptr));
    }
  } else if (typeid(*src_) == typeid(ConstExp)) {
    // dst非MEM,src CONST
    auto src2cst = static_cast<ConstExp *>(src_);
    std::ostringstream assem;
    assem << "movq $" << src2cst->consti_ << ",`d0";
    instr_list.Append(new assem::OperInstr(
        assem.str(), new temp::TempList(dst_->Munch(instr_list, fs)),
        new temp::TempList(), nullptr));
  } else {
    // dst和mem均非MEM和CONST
    instr_list.Append(new assem::MoveInstr(
        "movq `s0,`d0", new temp::TempList(dst_->Munch(instr_list, fs)),
        new temp::TempList(src_->Munch(instr_list, fs))));
  }
}

// ExpStm直接指令覆盖即可
void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  this->exp_->Munch(instr_list, fs);
}

// 从此处开始的所有语句都是有返回值的，其执行的结果返回至一个Temp变量中，可以被父级变量使用
// 二元运算
temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  auto left_temp = this->left_->Munch(instr_list, fs);
  auto right_temp = this->right_->Munch(instr_list, fs);
  auto result_temp = temp::TempFactory::NewTemp();
  switch (this->op_) {
  case PLUS_OP: {
    if (left_temp == reg_manager->FramePointer()) {
      // 计算栈帧值时，我们不从%rbp中取值，而是直接根据栈的大小+rsp位置来计算
      // （因为每一个函数栈帧在进入一开始便被分配，后续这个栈帧的大小是不变的）
      std::ostringstream str;
      str << "leaq " << fs << "_framesize(%rsp),`d0";
      instr_list.Append(new assem::OperInstr(str.str(),
                                             new temp::TempList(result_temp),
                                             new temp::TempList(), nullptr));
    } else {
      // 普通计算
      instr_list.Append(new assem::MoveInstr("movq `s0,`d0",
                                             new temp::TempList(result_temp),
                                             new temp::TempList(left_temp)));
    }
    instr_list.Append(
        new assem::OperInstr("addq `s0,`d0", new temp::TempList(result_temp),
                             new temp::TempList{right_temp}, nullptr));
    return result_temp;
  }
  case MINUS_OP: {
    instr_list.Append(new assem::MoveInstr("movq `s0,`d0",
                                           new temp::TempList(result_temp),
                                           new temp::TempList(left_temp)));
    // NOTE:
    instr_list.Append(
        new assem::OperInstr("subq `s0,`d0", new temp::TempList(result_temp),
                             new temp::TempList{right_temp}, nullptr));
    return result_temp;
  }
  case MUL_OP: {
    // 将rax的值乘src的值，结果存储在rdx：rax中
    auto rax = reg_manager->GetRegister(0);
    auto rdx = reg_manager->GetRegister(3);
    instr_list.Append(new assem::MoveInstr("movq `s0,%rax",
                                           new temp::TempList(rax),
                                           new temp::TempList(left_temp)));
    instr_list.Append(
        new assem::OperInstr("imulq `s0", new temp::TempList{rax, rdx},
                             new temp::TempList{right_temp, rax}, nullptr));
    return rax;
  }
  case DIV_OP: {
    // 将被除数移入rax,使用cqto扩展至rdx：rax,后除以src值
    // 商存储在rax中，余数存储在rdx中
    auto rax = reg_manager->GetRegister(0);
    auto rdx = reg_manager->GetRegister(3);
    instr_list.Append(new assem::MoveInstr("movq `s0,%rax",
                                           new temp::TempList(rax),
                                           new temp::TempList(left_temp)));
    instr_list.Append(new assem::OperInstr("cqto", new temp::TempList(rdx),
                                           new temp::TempList(), nullptr));
    instr_list.Append(new assem::OperInstr(
        "idivq `s0", new temp::TempList{rax, rdx},
        new temp::TempList{right_temp, rax, rdx}, nullptr));
    return rax;
  }
  }
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  auto return_temp = temp::TempFactory::NewTemp();
  if (typeid(*(this->exp_)) == typeid(BinopExp)) {
    auto exp_binop = static_cast<BinopExp *>(this->exp_);
    if (exp_binop->op_ == PLUS_OP) {
      if (typeid(*(exp_binop->left_)) == typeid(ConstExp)) {
        // MEM、+、左值CONST
        auto left_const = static_cast<ConstExp *>(exp_binop->left_);
        auto right_temp = exp_binop->right_->Munch(instr_list, fs);
        std::ostringstream str;
        if (right_temp == reg_manager->FramePointer()) {
          // 如果右侧需要栈指针，则需要使用其fs操作
          str << "movq " << fs << "_framesize" << left_const->consti_
              << "(`s0),`d0";
          right_temp = reg_manager->StackPointer();
        } else {
          str << "movq " << left_const->consti_ << "(`s0),`d0";
        }
        instr_list.Append(
            new assem::OperInstr(str.str(), new temp::TempList(return_temp),
                                 new temp::TempList(right_temp), nullptr));
      } else if (typeid(*(exp_binop->right_)) == typeid(ConstExp)) {
        // MEM、+、右值CONST 内容与上一个基本一致
        auto right_const = static_cast<ConstExp *>(exp_binop->right_);
        auto left_temp = exp_binop->left_->Munch(instr_list, fs);
        std::ostringstream assem;
        if (left_temp == reg_manager->FramePointer()) {
          assem << "movq " << fs << "_framesize" << right_const->consti_
                << "(`s0),`d0";
          left_temp = reg_manager->StackPointer();
        } else {
          assem << "movq " << right_const->consti_ << "(`s0),`d0";
        }
        instr_list.Append(
            new assem::OperInstr(assem.str(), new temp::TempList(return_temp),
                                 new temp::TempList(left_temp), nullptr));
      } else {
        instr_list.Append(new assem::OperInstr(
            "movq (`s0),`d0", new temp::TempList(return_temp),
            new temp::TempList(this->exp_->Munch(instr_list, fs)), nullptr));
      }
    }
  } else if (typeid(*(this->exp_)) == typeid(ConstExp)) {
    // MEM、CONST
    // 没在X64中发现这种用法
  } else {
    // MEM
    instr_list.Append(new assem::OperInstr(
        "movq (`s0),`d0", new temp::TempList(return_temp),
        new temp::TempList(exp_->Munch(instr_list, fs)), nullptr));
  }
  return return_temp;
}

// TempExp 直接返回其temp供父语句使用即可
temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  return this->temp_;
}

// 顺次执行指令覆盖，但要返回最后一条语句的值
temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  this->stm_->Munch(instr_list, fs);
  return this->exp_->Munch(instr_list, fs);
}

// 使用NameExp的可能有三种：call的函数名，jump的跳转标签，字符串常量标签
// 前两种的NameExp都随着call和jump本身对标签的解析而被解析了，没有单独语句
// 故本Munch只解析第三中，即获得字符串常量的位置指针
temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // 参考https://www.cnblogs.com/papertree/p/6298763.html 理解L(%rip)
  auto string_temp = temp::TempFactory::NewTemp();
  std::ostringstream str;
  str << "leaq " << this->name_->Name() << "(%rip),`d0";
  instr_list.Append(new assem::OperInstr(str.str(),
                                         new temp::TempList(string_temp),
                                         new temp::TempList(), nullptr));
  return string_temp;
}

// 常量语句：将常量移入一个Temp中返回
temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  auto num_temp = temp::TempFactory::NewTemp();
  std::ostringstream str;
  str << "movq $" << consti_ << ",`d0";
  instr_list.Append(new assem::OperInstr(
      str.str(), new temp::TempList(num_temp), new temp::TempList(), nullptr));
  return num_temp;
}

// 函数调用函数
temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  size_t args_size = this->args_->GetList().size();
  // 如果函数参数大于6,需要在当前栈帧上临时开出一块空间用于传递剩余的参数
  if (args_size > 6) {
    std::ostringstream str;
    str << "subq $" << (args_size - 6) * reg_manager->WordSize() << ",%rsp";
    instr_list.Append(new assem::OperInstr(str.str(), new temp::TempList(),
                                           new temp::TempList(), nullptr));
  }
  auto callersave_with_return = reg_manager->CallerSaves();
  callersave_with_return->Append(reg_manager->ReturnValue());
  auto args_list = this->args_->MunchArgs(instr_list, fs);
  std::ostringstream str;
  str << "callq " << static_cast<NameExp *>(this->fun_)->name_->Name();
  instr_list.Append(new assem::OperInstr(str.str(), callersave_with_return,
                                         args_list, nullptr));
  if (args_size > 6) {
    std::ostringstream str;
    str << "addq $" << (args_size - 6) * reg_manager->WordSize() << ",%rsp";
    instr_list.Append(new assem::OperInstr(str.str(), new temp::TempList(),
                                           new temp::TempList(), nullptr));
  }
  return reg_manager->ReturnValue();
}

// ExpList只会在Call的传参列表中被使用
temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list,
                                   std::string_view fs) {
  /* TODO: Put your lab5 code here */
  auto temp_list = new temp::TempList();
  auto exps_size = this->exp_list_.size();
  // 记录新开辟的传参栈空间
  int moresize = (exps_size - 6) * reg_manager->WordSize();
  if (moresize < 0) {
    moresize = 0;
  }
  // 获得寄存器分配器的传参列表
  auto arg_list = reg_manager->ArgRegs();
  // 参数index
  int i = 0;
  for (auto exp_item : this->exp_list_) {
    // 解析每个传入参数
    auto exp_temp = exp_item->Munch(instr_list, fs);
    switch (i) {
    case 0: {
      temp_list->Append(arg_list->NthTemp(0));
      if (exp_temp == reg_manager->FramePointer()) {
        // 第一个传参可能是帧指针
        std::ostringstream str;
        str << "leaq (" << fs << "_framesize+" << moresize << ")(%rsp),`d0";
        instr_list.Append(new assem::OperInstr(
            str.str(), new temp::TempList(arg_list->NthTemp(0)),
            new temp::TempList(), nullptr));
      } else {
        // 外部函数第一参数不是帧指针
        instr_list.Append(new assem::MoveInstr(
            "movq `s0,`d0", new temp::TempList(arg_list->NthTemp(0)),
            new temp::TempList(exp_temp)));
      }
      break;
    }
    case 1:
    case 2:
    case 3:
    case 4:
    case 5: {
      temp_list->Append(arg_list->NthTemp(i));
      instr_list.Append(new assem::MoveInstr(
          "movq `s0,`d0", new temp::TempList(arg_list->NthTemp(i)),
          new temp::TempList(exp_temp)));
      break;
    }
    default: {
      std::ostringstream str;
      str << "movq `s0," << (i - 6) * reg_manager->WordSize() << "(%rsp)";
      instr_list.Append(new assem::OperInstr(str.str(), new temp::TempList(),
                                             new temp::TempList(exp_temp),
                                             nullptr));
      break;
    }
    }
    ++i;
  }
  return temp_list;
}

} // namespace tree
