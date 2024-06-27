#include "tiger/liveness/flowgraph.h"
#include <map>

namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  /* TODO: Put your lab6 code here */
  flowgraph_ = new graph::Graph<assem::Instr>;
  std::map<temp::Label *, FNodePtr> label2fnode;
  // 指令与FNode的映射关系表
  std::list<std::pair<assem::Instr *, FNodePtr>> jump2fnode;
  FNodePtr last = nullptr;
  for (auto instr : instr_list_->GetList()) {
    auto cur = flowgraph_->NewNode(instr);
    if (last)
      // 如果上一条“拥有连接顺次的”指令存在（没有此字段为null），将上一条指令与本条指令连接
      flowgraph_->AddEdge(last, cur);
    if (typeid(*instr) == typeid(assem::LabelInstr)) {
      // 如果是标签语句,记录label到语句的映射关系
      label2fnode[static_cast<assem::LabelInstr *>(instr)->label_] = cur;
      last = cur;
    } else if (typeid(*instr) == typeid(assem::OperInstr) &&
               static_cast<assem::OperInstr *>(instr)->jumps_) {
      // Jump OR Cjump
      // 跳转语句：将跳转语句加入链表中，等待下一步处理
      if (static_cast<assem::OperInstr *>(instr)->assem_.find(
              std::string("jmp")) != std::string::npos) {
        // jump 直接后继不需要与其连接
        last = nullptr;
        jump2fnode.emplace_back(instr, cur);
      } else {
        // cjump 直接后继需要与其连接
        last = cur;
        jump2fnode.emplace_back(instr, cur);
      }
    } else {
      // 普通语句无需进行额外操作，等待下一循环与下一条语句连接即可
      last = cur;
    }
  }

  for (auto instr_fnode : jump2fnode) {
    auto jump_instr = static_cast<assem::OperInstr *>(instr_fnode.first);
    for (auto label : *jump_instr->jumps_->labels_) {
      // 为每个跳转指令中所有的跳转目标连接跳转边
      flowgraph_->AddEdge(instr_fnode.second, label2fnode[label]);
    }
  }
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_;
}

temp::TempList *MoveInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_;
}

temp::TempList *OperInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_;
}

temp::TempList *LabelInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_;
}

temp::TempList *MoveInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_;
}

temp::TempList *OperInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_;
}
} // namespace assem
