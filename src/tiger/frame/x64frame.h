//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {
class X64RegManager : public RegManager {
  /* TODO: Put your lab5 code here */
public:
  X64RegManager();
  // 按序返回所有寄存器
  temp::TempList *Registers() override;
  // 按序返回所有传参寄存器
  temp::TempList *ArgRegs() override;
  // 按序返回所有caller save寄存器
  temp::TempList *CallerSaves() override;
  // 按序返回所有callee save寄存器
  temp::TempList *CalleeSaves() override;
  // 返回返回值寄存器
  temp::TempList *ReturnSink() override;

  // 返回一个字的大小，这里固定为8
  int WordSize() override;

  // 返回帧指针%rbp
  temp::Temp *FramePointer() override;
  // 返回栈指针%rsp
  temp::Temp *StackPointer() override;
  // 返回返回值寄存器%rax
  temp::Temp *ReturnValue() override;

  // MY_MODIFY:
  // 返回除了rsp以外的寄存器（rsp不用来着色）
  temp::TempList *AllWithoutRsp() override;
};

class X64Frame : public Frame {
  /* TODO: Put your lab5 code here */
public:
  X64Frame(temp::Label *name, std::list<bool> *formal_escape_list);
  ~X64Frame();
  Access *F_allocLocal(bool isEscape) override;
};

// 在栈上的变量包装Access
class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *getT_EXP(tree::Exp *fp) const override;
};

// 在临时变量（寄存器）上的变量包装Access
class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *getT_EXP(tree::Exp *fp) const override;
};

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
