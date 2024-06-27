#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/translate/tree.h"
#include <deque>
#include <list>
#include <memory>
#include <string>

namespace frame {

class RegManager {
public:
  RegManager() : temp_map_(temp::Map::Empty()) {}

  temp::Temp *GetRegister(int regno) { return regs_[regno]; }

  /**
   * Get general-purpose registers except RSI
   * NOTE: returned temp list should be in the order of calling convention
   * @return general-purpose registers
   */
  [[nodiscard]] virtual temp::TempList *Registers() = 0;

  /**
   * Get registers which can be used to hold arguments
   * NOTE: returned temp list must be in the order of calling convention
   * @return argument registers
   */
  [[nodiscard]] virtual temp::TempList *ArgRegs() = 0;

  /**
   * Get caller-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return caller-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CallerSaves() = 0;

  /**
   * Get callee-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return callee-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CalleeSaves() = 0;

  /**
   * Get return-sink registers
   * @return return-sink registers
   */
  [[nodiscard]] virtual temp::TempList *ReturnSink() = 0;

  /**
   * Get word size
   */
  [[nodiscard]] virtual int WordSize() = 0;

  [[nodiscard]] virtual temp::Temp *FramePointer() = 0;

  [[nodiscard]] virtual temp::Temp *StackPointer() = 0;

  [[nodiscard]] virtual temp::Temp *ReturnValue() = 0;

  temp::Map *temp_map_;

  // MY_MODIFY:
  [[nodiscard]] virtual temp::TempList *AllWithoutRsp() = 0;

protected:
  std::vector<temp::Temp *> regs_;
};

class Access {
public:
  /* TODO: Put your lab5 code here */
  virtual tree::Exp *getT_EXP(tree::Exp *frame_ptr) const = 0;
  virtual ~Access() = default;
};

class Frame {
  /* TODO: Put your lab5 code here */
public:
  // 记录当前栈的偏移量
  int current_stack_pos;
  // 记录此栈帧的名字
  temp::Label *name_;
  // 记录此栈帧上的变量表
  std::deque<frame::Access *> *formal_list;
  // 记录视角移位
  std::list<tree::Stm *> *view_shift_list;

  Frame(temp::Label *name, std::list<bool> *formal_escape_list) {
    this->current_stack_pos = -8;
    this->name_ = name;
    this->formal_list = new std::deque<frame::Access *>;
    this->view_shift_list = new std::list<tree::Stm *>;
  }
  virtual ~Frame() {
    delete formal_list;
    delete view_shift_list;
  };
  // 根据是否为逃逸变量，在栈帧上分配一个新的变量
  virtual Access *F_allocLocal(bool isEscape) = 0;
  std::string GetLabel() { return this->name_->Name(); }
};

/**
 * Fragments
 */

class Frag {
public:
  virtual ~Frag() = default;

  enum OutputPhase {
    Proc,
    String,
  };

  /**
   *Generate assembly for main program
   * @param out FILE object for output assembly file
   */
  virtual void OutputAssem(FILE *out, OutputPhase phase,
                           bool need_ra) const = 0;
};

class StringFrag : public Frag {
public:
  temp::Label *label_;
  std::string str_;

  StringFrag(temp::Label *label, std::string str)
      : label_(label), str_(std::move(str)) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class ProcFrag : public Frag {
public:
  tree::Stm *body_;
  Frame *frame_;

  ProcFrag(tree::Stm *body, Frame *frame) : body_(body), frame_(frame) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class Frags {
public:
  Frags() = default;
  void PushBack(Frag *frag) { frags_.emplace_back(frag); }
  const std::list<Frag *> &GetList() { return frags_; }

private:
  std::list<Frag *> frags_;
};

/* TODO: Put your lab5 code here */
tree::Exp *ExternalCall(std::string, tree::ExpList *args);

ProcFrag *ProcEntryExit1(frame::Frame *, tree::Stm *);

assem::InstrList *ProcEntryExit2(assem::InstrList *);

assem::Proc *ProcEntryExit3(frame::Frame *, assem::InstrList *);

} // namespace frame

#endif