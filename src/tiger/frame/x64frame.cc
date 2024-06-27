#include "tiger/frame/x64frame.h"
#include <sstream>

extern frame::RegManager *reg_manager;

namespace frame {
/* TODO: Put your lab5 code here */
X64RegManager::X64RegManager() {
  // 按照顺序创建寄存器临时变量，并入栈，入map
  std::vector<std::string *> x64_register_list = {
      new std::string("%rax"), new std::string("%rbx"), new std::string("%rcx"),
      new std::string("%rdx"), new std::string("%rsi"), new std::string("%rdi"),
      new std::string("%rbp"), new std::string("%rsp"), new std::string("%r8"),
      new std::string("%r9"),  new std::string("%r10"), new std::string("%r11"),
      new std::string("%r12"), new std::string("%r13"), new std::string("%r14"),
      new std::string("%r15")};
  for (int i = 0; i < 16; ++i) {
    temp::Temp *item = temp::TempFactory::NewTemp();
    this->regs_.push_back(item);
    this->temp_map_->Enter(item, x64_register_list[i]);
  }
}

temp::TempList *X64RegManager::Registers() {
  temp::TempList *result = new temp::TempList();
  for (temp::Temp *item : this->regs_) {
    result->Append(item);
  }
  return result;
}

temp::TempList *X64RegManager::ArgRegs() {
  return new temp::TempList({this->regs_[5], this->regs_[4], this->regs_[3],
                             this->regs_[2], this->regs_[8], this->regs_[9]});
}

temp::TempList *X64RegManager::CallerSaves() {
  return new temp::TempList({this->regs_[0], this->regs_[10], this->regs_[11],
                             this->regs_[5], this->regs_[4], this->regs_[3],
                             this->regs_[2], this->regs_[8], this->regs_[9]});
}

temp::TempList *X64RegManager::CalleeSaves() {
  return new temp::TempList({this->regs_[1], this->regs_[6], this->regs_[12],
                             this->regs_[13], this->regs_[14],
                             this->regs_[15]});
}

temp::TempList *X64RegManager::ReturnSink() {
  temp::TempList *callee_save_list = this->CalleeSaves();
  callee_save_list->Append(this->StackPointer());
  callee_save_list->Append(this->ReturnValue());
  return callee_save_list;
}

int X64RegManager::WordSize() { return 8; }

temp::Temp *X64RegManager::FramePointer() {
  // %rbp
  return this->regs_[6];
}

temp::Temp *X64RegManager::StackPointer() {
  // %rsp
  return this->regs_[7];
}

temp::Temp *X64RegManager::ReturnValue() {
  // %rax
  return this->regs_[0];
}

tree::Exp *InFrameAccess::getT_EXP(tree::Exp *fp) const {
  return new tree::MemExp(new tree::BinopExp(tree::BinOp::PLUS_OP, fp,
                                             new tree::ConstExp(this->offset)));
}

tree::Exp *InRegAccess::getT_EXP(tree::Exp *fp) const {
  return new tree::TempExp(this->reg);
}

X64Frame::X64Frame(temp::Label *name, std::list<bool> *formal_escape_list)
    : Frame(name, formal_escape_list) {
  // 至少有一个静态链的bool值
  if (formal_escape_list == nullptr) {
    return;
  }
  // 为formal_escape_list的静态链位移和参数值分配栈帧上空间
  for (bool escape_item : *formal_escape_list) {
    this->formal_list->push_back(this->F_allocLocal(escape_item));
  }
  size_t formal_list_size = this->formal_list->size();
  // 取帧指针对应的树
  tree::TempExp *fp = new tree::TempExp(reg_manager->FramePointer());
  // 视角位移：将参数从被调用者搬至调用者，前6个是寄存器传参，后面是栈传参
  for (int i = 0; i < formal_list_size; ++i) {
    tree::MoveStm *move_stm;
    if (i <= 5) {
      move_stm = new tree::MoveStm(
          (*(this->formal_list))[i]->getT_EXP(fp),
          new tree::TempExp(reg_manager->ArgRegs()->NthTemp(i)));
    } else {
      move_stm = new tree::MoveStm(
          (*(this->formal_list))[i]->getT_EXP(fp),
          new tree::MemExp(new tree::BinopExp(
              tree::BinOp::PLUS_OP, fp,
              new tree::ConstExp(
                  reg_manager->WordSize() *
                  (i - 6 + 1))))); // 因为返回地址的存在，所以额外+1
      // X64 call 指令将会先推入return address, 随后进行控制转移和帧指针改变
    }
    // 记录视角移位序列
    this->view_shift_list->push_back(move_stm);
  }
}

X64Frame::~X64Frame() {
  delete this->formal_list;
  delete this->view_shift_list;
}

Access *X64Frame::F_allocLocal(bool isEscape) {
  Access *result;
  // 如果是逃逸变量，则分配在栈上，并将栈栈顶位置下移一个字（8B）
  if (isEscape) {
    result = new InFrameAccess(current_stack_pos);
    this->current_stack_pos -= reg_manager->WordSize();
  } else {
    // 否则优先考虑放在寄存器上，分配一个新的临时变量用以保存
    result = new InRegAccess(temp::TempFactory::NewTemp());
  }
  return result;
}

ProcFrag *ProcEntryExit1(frame::Frame *frame, tree::Stm *stm) {
  auto vs_it = frame->view_shift_list->begin();
  auto vs_end = frame->view_shift_list->end();

  // 无视角移位，无需添加
  if (vs_it == vs_end) {
    return new ProcFrag(stm, frame);
  }

  // 同Translate操作，如果只有一个视角移位语句，则不需要包裹一层front_stm
  auto front_stm = *(vs_it);
  ++vs_it;
  while (vs_it != vs_end) {
    front_stm = new tree::SeqStm(front_stm, *vs_it);
    ++vs_it;
  }
  stm = new tree::SeqStm(front_stm, stm);

  // 返回的是先经过视角移位处理后的函数语句
  return new ProcFrag(stm, frame);
}

assem::InstrList *ProcEntryExit2(assem::InstrList *body) {
  // body->Append(new assem::OperInstr("", new temp::TempList(),
  //                                   reg_manager->ReturnSink(), nullptr));
  return body;
}

tree::Exp *ExternalCall(std::string fun, tree::ExpList *args) {
  auto label = temp::LabelFactory::NamedLabel(fun);
  return new tree::CallExp(new tree::NameExp(label), args);
}

// 生成过程入口处理和出口处理汇编代码
assem::Proc *ProcEntryExit3(frame::Frame *frame, assem::InstrList *body) {
  std::ostringstream prologue;
  // 此处的栈帧计算原因见compiler_lab.md
  int framesize = -(frame->current_stack_pos + reg_manager->WordSize());
  prologue << ".set " << frame->name_->Name() << "_framesize, " << framesize
           << "\n";
  // 函数标签，用以跳转
  prologue << frame->name_->Name() << ":\n";
  // 为该函数分配栈空间
  prologue << "subq $" << framesize << ",%rsp\n";
  std::ostringstream epilogue;
  // 收回该函数的栈空间
  epilogue << "addq $" << framesize << ",%rsp\n";
  // 返回ProcEntryExit3
  epilogue << "retq\n";
  // 入口处理 函数体 出口处理三合一
  return new assem::Proc(prologue.str(), body, epilogue.str());
}

temp::TempList *X64RegManager::AllWithoutRsp() {
  //  被调用者寄存器在后
  return new temp::TempList{regs_[0],  regs_[2],  regs_[3], regs_[4],
                            regs_[5],  regs_[8],  regs_[9], regs_[10],
                            regs_[11], regs_[1],  regs_[6], regs_[12],
                            regs_[13], regs_[14], regs_[15]};
}

} // namespace frame
