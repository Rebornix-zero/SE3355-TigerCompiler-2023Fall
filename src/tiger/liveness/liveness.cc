#include "tiger/liveness/liveness.h"

extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  // 检查movelist中是否包含一个指定的元素
  for (auto item : this->move_list_) {
    if (item.first == src && item.second == dst) {
      return true;
    }
  }
  return false;
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  for (auto move : list->GetList()) {
    if (!res->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  // 将两个MoveList取交集，返回一个新的MoveList
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

// MY_MODIFY:
bool Contains(temp::TempList *base, temp::Temp *target) {
  assert(base);
  auto temp_list = base->GetList();
  return std::find(temp_list.begin(), temp_list.end(), target) !=
         temp_list.end();
}

temp::TempList *Union(temp::TempList *lhs, temp::TempList *rhs) {
  if (lhs == nullptr) {
    if (rhs == nullptr)
      return new temp::TempList();
    return new temp::TempList(rhs->GetList());
  }
  auto union_list = new temp::TempList(lhs->GetList());
  for (auto temp : rhs->GetList()) {
    if (!Contains(union_list, temp))
      union_list->Append(temp);
  }
  return union_list;
}

// 从一个集合中去掉另一个集合中包含的元素
temp::TempList *Subtract(temp::TempList *lhs, temp::TempList *rhs) {
  if (lhs == nullptr) {
    return new temp::TempList();
  }
  auto result_list = new temp::TempList(lhs->GetList());
  if (rhs != nullptr) {
    for (auto temp : rhs->GetList()) {
      result_list->Remove(temp);
    }
  }
  return result_list;
}

bool Equal(temp::TempList *lhs, temp::TempList *rhs) {
  if (lhs == nullptr) {
    if (rhs == nullptr)
      return true;
    return false;
  }
  if (rhs == nullptr)
    return false;

  for (auto temp : lhs->GetList()) {
    if (!Contains(rhs, temp)) {
      return false;
    }
  }
  return true;
}

void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */
  // 为每条语句的node初始化其in和out
  for (auto flow_node : flowgraph_->Nodes()->GetList()) {
    in_->Enter(flow_node, new temp::TempList());
    out_->Enter(flow_node, new temp::TempList());
  }

  // NOTE:活跃分析迭代将反向进行
  auto end_flag = flowgraph_->Nodes()->GetList().rend();
  auto first = flowgraph_->Nodes()->GetList().rbegin();
  while (true) {
    bool is_all_same = true;
    for (auto flow_node_it = first; flow_node_it != end_flag; flow_node_it++) {
      // 保存老的in和out
      auto old_in = in_->Look(*flow_node_it);
      auto old_out = out_->Look(*flow_node_it);
      // 从out中去掉def
      auto out_minus_def =
          Subtract(old_out, (*flow_node_it)->NodeInfo()->Def());
      // 建立新的in
      auto new_in = Union((*flow_node_it)->NodeInfo()->Use(), out_minus_def);

      // 遍历后继节点的in,联合，建立新的out
      auto succ_it = (*flow_node_it)->Succ()->GetList().begin();
      auto succ_end = (*flow_node_it)->Succ()->GetList().end();
      temp::TempList *new_out = new temp::TempList();
      temp::TempList *last_out = nullptr;
      for (auto it = succ_it; it != succ_end; ++it) {
        last_out = new_out;
        new_out = Union(new_out, in_->Look(*it));
        // NOTE:Union每次会根据第一参数重新构造一个新的TempList，故前者需要delete
        delete last_out;
      }
      // 比较本次更新是否引发了in和out的改变
      if (!Equal(new_in, old_in) || !Equal(new_out, old_out)) {
        is_all_same = false;
      }
      in_->Enter(*flow_node_it, new_in);
      delete old_in;
      out_->Enter(*flow_node_it, new_out);
      delete old_out;
    }
    // 如果一轮迭代中没有任何改变，则可以退出活跃分析迭代
    if (is_all_same)
      break;
  }
}

void LiveGraphFactory::InterfGraph() {
  /* TODO: Put your lab6 code here */
  // 加入预定义的机器寄存器节点
  // 注意，rsp将不会放入冲突图中，我们使其单独独立
  auto all_without_rsp = reg_manager->AllWithoutRsp();
  for (auto temp : all_without_rsp->GetList()) {
    temp_node_map_[temp] = live_graph_.interf_graph->NewNode(temp);
  }

  // 所有机器寄存器节点之间相互冲突
  auto temp_it = all_without_rsp->GetList().begin();
  auto temp_end = all_without_rsp->GetList().end();
  for (; temp_it != temp_end; temp_it++) {
    for (auto temp_inner_it = std::next(temp_it); temp_inner_it != temp_end;
         temp_inner_it++) {
      live_graph_.interf_graph->AddEdge(temp_node_map_[*temp_it],
                                        temp_node_map_[*temp_inner_it]);
      live_graph_.interf_graph->AddEdge(temp_node_map_[*temp_inner_it],
                                        temp_node_map_[*temp_it]);
    }
  }
  delete all_without_rsp;

  // 将out包含的用过的临时变量节点加入
  auto nodes = flowgraph_->Nodes()->GetList();
  for (auto flow_node : nodes) {
    auto out_temps = out_->Look(flow_node)->GetList();
    for (auto temp : out_temps) {
      if (temp_node_map_.find(temp) == temp_node_map_.end())
        temp_node_map_[temp] = live_graph_.interf_graph->NewNode(temp);
    }
  }

  // 为临时加入变量构建冲突
  auto rsp = reg_manager->StackPointer();
  for (auto flow_node : nodes) {
    auto assem = flow_node->NodeInfo();
    auto live = out_->Look(flow_node);
    if (typeid(*assem) == typeid(assem::MoveInstr)) {
      // 传送指令，对新定义的temp做与非src的out之间的冲突检查
      // 在codegen中，我们确保了MoveInstr指令的def和use均只有一个
      // 传送指令中，def和use之间不应该添加冲突线，而是move的虚线
      auto out_minus_use = Subtract(live, assem->Use());
      for (auto def : assem->Def()->GetList()) {
        // 如果函数只定义而未用过，其将在之前未加入冲突图中，应该加入（虽然不会与除了本指令use的其他临时变量产生任何关系，因为其活跃区间仅限于这一条指令）
        if (temp_node_map_.find(def) == temp_node_map_.end()) {
          temp_node_map_[def] = live_graph_.interf_graph->NewNode(def);
        }
        // 为非use的out与def间添加冲突
        if (def == rsp)
          continue;
        for (auto out : out_minus_use->GetList()) {
          if (out == rsp)
            continue;
          live_graph_.interf_graph->AddEdge(temp_node_map_[def],
                                            temp_node_map_[out]);
          live_graph_.interf_graph->AddEdge(temp_node_map_[out],
                                            temp_node_map_[def]);
        }
        // 为该条MoveInstr的def和use添加move，方便以后尽可能分配同一个寄存器
        for (auto use : assem->Use()->GetList()) {
          if (use == rsp)
            continue;
          if (!live_graph_.moves->Contain(temp_node_map_[def],
                                          temp_node_map_[use]) &&
              !live_graph_.moves->Contain(temp_node_map_[use],
                                          temp_node_map_[def])) {
            live_graph_.moves->Append(temp_node_map_[def], temp_node_map_[use]);
          }
        }
      }
    } else {
      // 非传送指令，对新定义的temp做与out之间的冲突检查
      for (auto def : assem->Def()->GetList()) {
        if (def == rsp)
          continue;
        for (auto out : live->GetList()) {
          if (out == rsp)
            continue;
          live_graph_.interf_graph->AddEdge(temp_node_map_[def],
                                            temp_node_map_[out]);
          live_graph_.interf_graph->AddEdge(temp_node_map_[out],
                                            temp_node_map_[def]);
        }
      }
    }
  }
}

void LiveGraphFactory::Liveness() {
  // 构建活跃表
  LiveMap();
  // 构建冲突图
  InterfGraph();
}

} // namespace live
