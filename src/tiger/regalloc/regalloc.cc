#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"
#include <iostream>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace ra {
/* TODO: Put your lab6 code here */
RegAllocator::RegAllocator(frame::Frame *frame,
                           std::unique_ptr<cg::AssemInstr> assem_instr)
    : frame_(frame), assem_instr_(std::move(assem_instr)) {
  result_ = std::make_unique<Result>();
}

void RegAllocator::RegAlloc() {
  while (true) {
    Build();
    MakeWorklist();
    // 四个工作表中只要有非空部分，则要进行如下操作：
    // 顺序分别是：简化--合并--冻结--溢出
    while (!simplifyWorklist->Empty() || !worklistMoves->Empty() ||
           !freezeWorklist->Empty() || !spillWorklist->Empty()) {
      if (!simplifyWorklist->Empty()) {
        Simplify();
      } else if (!worklistMoves->Empty()) {
        Coalesce();
      } else if (!freezeWorklist->Empty()) {
        Freeze();
      } else if (!spillWorklist->Empty())
        SelectSpill();
    }

    // 直至工作表全空，这代表原冲突图中除了预着色节点外已经全部被压入栈中，可以逐步出栈并着色
    AssignColors();

    if (!spilledNodes->Empty()) {
      // 根据溢出节点表重写程序
      RewriteProgram();
      // 清空状态，重新build
      Reset();
      movelist.clear();
    } else {
      // 提取结果
      result_->coloring_ = AssignRegisters();
      result_->il_ = assem_instr_.get()->GetInstrList();
      break;
    }
  }
}

void RegAllocator::Build() {
  auto flow_graph_factory = fg::FlowGraphFactory(assem_instr_->GetInstrList());
  // 构建流图
  flow_graph_factory.AssemFlowGraph();
  // 返回流图
  live_graph_factory_ =
      new live::LiveGraphFactory(flow_graph_factory.GetFlowGraph());
  // 活跃分析
  live_graph_factory_->Liveness();
  // 返回活跃分析结果，包含冲突图和move pair，用于展示某个点初始的传送情况
  live_graph = live_graph_factory_->GetLiveGraph();
  worklistMoves = live_graph->moves;
  // 获取temp对应的graph节点映射图
  auto temp2Inode = live_graph_factory_->GetTempNodeMap();

  simplifyWorklist = new live::INodeList();
  freezeWorklist = new live::INodeList();
  spillWorklist = new live::INodeList();
  spilledNodes = new live::INodeList();
  coalescedNodes = new live::INodeList();
  coloredNodes = new live::INodeList();
  selectStack = new live::INodeList();

  activeMoves = new live::MoveList();
  coalescedMoves = new live::MoveList();
  frozenMoves = new live::MoveList();
  constrainedMoves = new live::MoveList();

  // 预着色节点着色
  for (auto reg : reg_manager->AllWithoutRsp()->GetList()) {
    coloredNodes->Append(temp2Inode[reg]);
    color[temp2Inode[reg]] = reg;
  }

  auto nodes = live_graph->interf_graph->Nodes();
  for (auto node : nodes->GetList()) {
    auto new_movelist = new live::MoveList();
    for (auto move : worklistMoves->GetList()) {
      if (move.first == node || move.second == node) {
        new_movelist->Append(move.first, move.second);
      }
    }
    // 节点的传送相关表（判定是否还与传送相关）
    movelist[node] = new_movelist;
    // 节点的度（判定是否为高度数点）
    degree[node] = node->Degree();
  }
}

bool RegAllocator::AddEdge(live::INodePtr u, live::INodePtr v) {
  if (!u->Succ()->Contain(v) && u != v) {
    live_graph->interf_graph->AddEdge(u, v);
    live_graph->interf_graph->AddEdge(v, u);
    degree[u]++;
    degree[v]++;
    return true;
  }
  return false;
}

void RegAllocator::MakeWorklist() {
  auto nodes = live_graph->interf_graph->Nodes();
  for (auto node : nodes->GetList()) {
    // 只包含非预着色节点（预着色节点已经被染色，从而不加入下列工作区中）
    if (!coloredNodes->Contain(node)) {
      std::cout << node->Degree() << std::endl;
      if (node->Degree() >= K)
        // 高度数节点
        spillWorklist->Append(node);
      else if (MoveRelated(node))
        // 低度数传送有关节点
        freezeWorklist->Append(node);
      else
        // 低度数传送无关节点
        simplifyWorklist->Append(node);
    }
  }
  std::cout << std::endl;
}

live::INodeListPtr RegAllocator::Adjacent(live::INodePtr node) {
  // 参考课本给出的寄存器分配临界算法
  // 已经入栈的节点，已经被合并的节点均不算临近
  auto raw_adj = node->Adj();
  auto union_list = selectStack->Union(coalescedNodes);
  auto real_adj = raw_adj->Diff(union_list);
  delete union_list;
  return real_adj;
}

live::MoveList *RegAllocator::NodeMoves(live::INodePtr node) {
  // 全部的MoveList
  auto union_list = activeMoves->Union(worklistMoves);
  // 与node节点对应的movelist取交集
  auto result = movelist[node]->Intersect(union_list);
  delete union_list;
  return result;
}

bool RegAllocator::MoveRelated(live::INodePtr node) {
  // 若交集出来是空，则代表其不是move相关的，否则是move相关
  auto node_moves = NodeMoves(node);
  bool is_empty = node_moves->GetList().empty();
  delete node_moves;
  return !is_empty;
}

void RegAllocator::DecrementDegree(live::INodePtr node) {
  int d = degree[node];
  degree[node] = d - 1;
  if (d == K) {
    // 该节点减完后由高度数节点变为低度数节点，需要转移工作列表
    auto m_with_adj = Adjacent(node);
    m_with_adj->Append(node);
    EnableMoves(m_with_adj);
    spillWorklist->DeleteNode(node);
    if (MoveRelated(node))
      freezeWorklist->Append(node);
    else
      simplifyWorklist->Append(node);
  }
}

void RegAllocator::EnableMoves(live::INodeListPtr nodes) {
  for (auto node : nodes->GetList()) {
    auto node_moves = NodeMoves(node);
    for (auto pair_node : node_moves->GetList()) {
      if (activeMoves->Contain(pair_node.first, pair_node.second)) {
        activeMoves->Delete(pair_node.first, pair_node.second);
        worklistMoves->Append(pair_node.first, pair_node.second);
      }
    }
    delete node_moves;
  }
}

void RegAllocator::Simplify() {
  // 取一个节点入栈，并从worklist中删除
  auto node = simplifyWorklist->GetList().front();
  simplifyWorklist->DeleteNode(node);
  selectStack->Prepend(node);
  // 将对应冲突图中所有临近点的度减1
  auto adjacent = Adjacent(node);
  for (auto adj_node : adjacent->GetList()) {
    DecrementDegree(adj_node);
  }
  delete adjacent;
}

void RegAllocator::Coalesce() {
  // 取一组可合并节点
  auto m = worklistMoves->GetList().front();
  auto x = GetAlias(m.first);
  auto y = GetAlias(m.second);
  // 实际的合并节点，并指明合并的方向:u<-v（有颜色的节点应该接受无颜色节点的合并）
  live::INodePtr u, v;
  if (coloredNodes->Contain(y)) {
    u = y;
    v = x;
  } else {
    u = x;
    v = y;
  }
  // 删除对应的可合并节点组
  worklistMoves->Delete(m.first, m.second);
  if (u == v) {
    coalescedMoves->Append(m.first, m.second);
    AddWorkList(u);
  } else if (coloredNodes->Contain(v) || u->Succ()->Contain(v)) {
    // 冲突
    constrainedMoves->Append(m.first, m.second);
    AddWorkList(u);
    AddWorkList(v);
  } else if ((coloredNodes->Contain(u) && OK(v, u)) ||
             (!coloredNodes->Contain(u) && Conservative(u, v))) {
    // 如果u是预着色节点，或者u未着色的情况下允许保守合并
    coalescedMoves->Append(m.first, m.second);
    Combine(u, v);
    AddWorkList(u);
  } else
    activeMoves->Append(m.first, m.second);
}

live::INodePtr RegAllocator::GetAlias(live::INodePtr node) {
  // 如果其是已经合并的，将返回合并后的节点，否则之直接返回
  if (coalescedNodes->Contain(node))
    return GetAlias(alias[node]);
  else
    return node;
}

void RegAllocator::AddWorkList(live::INodePtr node) {
  // 如果节点无着色，无移动相关，低度，则可以将其加入低度无传送节点表，并在低度有传送节点表中删除相关项
  if (!coloredNodes->Contain(node) && !MoveRelated(node) && degree[node] < K) {
    freezeWorklist->DeleteNode(node);
    simplifyWorklist->Append(node);
  }
}

bool RegAllocator::OK(live::INodePtr v, live::INodePtr u) {
  auto v_adj = Adjacent(v);
  for (auto t : v_adj->GetList()) {
    if (!(degree[t] < K || coloredNodes->Contain(t) || t->Succ()->Contain(u))) {
      delete v_adj;
      return false;
    }
  }
  delete v_adj;
  return true;
}

bool RegAllocator::Conservative(live::INodePtr u, live::INodePtr v) {
  // 使用Briggs算法判定是否可以保守合并
  auto adj_u = Adjacent(u);
  auto adj_v = Adjacent(v);
  auto v_union_u = adj_u->Union(adj_v);
  int k = 0;
  for (auto node : v_union_u->GetList()) {
    if (degree[node] >= K)
      k++;
  }
  delete adj_u;
  delete adj_v;
  delete v_union_u;
  return k < K;
}

void RegAllocator::Combine(live::INodePtr u, live::INodePtr v) {
  // 将两个节点合并
  if (freezeWorklist->Contain(v)) {
    freezeWorklist->DeleteNode(v);
  } else {
    spillWorklist->DeleteNode(v);
  }
  coalescedNodes->Append(v);
  alias[v] = u;
  // 传送指令关系合并
  auto u_moves = movelist[u];
  movelist[u] = u_moves->Union(movelist[v]);
  graph::NodeList<temp::Temp> v_list;
  v_list.Append(v);
  EnableMoves(&v_list);
  delete u_moves;
  // 修改合并后节点对临界节点的影响
  auto v_adj = Adjacent(v);
  for (auto t : v_adj->GetList()) {
    AddEdge(t, u);
    DecrementDegree(t);
  }
  if (degree[u] >= K && freezeWorklist->Contain(u)) {
    // 经过Briggs算法验证后，不会出现合并导致不可K着色的情况
    // 但仍然可能会使合并节点高度数
    freezeWorklist->DeleteNode(u);
    spillWorklist->Append(u);
  }
}

void RegAllocator::Freeze() {
  // 将一个传送有关的低度数节点冻结，不再期望合并，从而使简化和合并可以重启
  auto u = freezeWorklist->GetList().front();
  freezeWorklist->DeleteNode(u);
  simplifyWorklist->Append(u);
  FreezeMoves(u);
}

void RegAllocator::FreezeMoves(live::INodePtr u) {
  auto node_moves = NodeMoves(u);
  for (auto move_pair : node_moves->GetList()) {
    live::INodePtr v;
    if (GetAlias(move_pair.second) == GetAlias(u))
      v = GetAlias(move_pair.first);
    else
      v = GetAlias(move_pair.second);
    // 将该节点对应的传送指令待合并对由activeMoves转移至frozenMoves
    activeMoves->Delete(move_pair.first, move_pair.second);
    frozenMoves->Append(move_pair.first, move_pair.second);
    // 如果删除move pair后，v节点变为低度传送无关指令，将其的work list转移
    if (!MoveRelated(v) && degree[v] < K) {
      freezeWorklist->DeleteNode(v);
      simplifyWorklist->Append(v);
    }
  }
  delete node_moves;
}

void RegAllocator::SelectSpill() {
  // 将溢出节点变为可入栈
  auto m = spillWorklist->GetList().front();
  spillWorklist->DeleteNode(m);
  simplifyWorklist->Append(m);
  FreezeMoves(m);
}

void RegAllocator::AssignColors() {
  for (auto n : selectStack->GetList()) {
    auto okColors = reg_manager->AllWithoutRsp();
    for (auto adj_node : n->Adj()->GetList()) {
      if (coloredNodes->Contain(GetAlias(adj_node)))
        // 删除邻近节点中已经染过的色
        // 注意，我们在有向图中一次添加来回两条边来实现无向图效果，故节点的所有后继节点即是临界节点
        okColors->Remove(color[GetAlias(adj_node)]);
    }
    if (okColors->GetList().empty()) {
      // 如果无法K染色，该节点为溢出节点
      spilledNodes->Append(n);
    } else {
      // 染色
      coloredNodes->Append(n);
      color[n] = okColors->GetList().front();
    }
  }

  for (auto n : coalescedNodes->GetList()) {
    // 更新被合并节点的颜色
    color[n] = color[GetAlias(n)];
  }
}

temp::TempList *replaceTempList(temp::TempList *temp_list, temp::Temp *old_temp,
                                temp::Temp *new_temp) {
  // 建立新的TempList，并替换掉其中的old temp
  auto new_temp_list = new temp::TempList();
  for (auto temp : temp_list->GetList()) {
    if (temp == old_temp) {
      new_temp_list->Append(new_temp);
    } else
      new_temp_list->Append(temp);
  }
  delete temp_list;
  return new_temp_list;
}

void RegAllocator::RewriteProgram() {
  for (auto node : spilledNodes->GetList()) {
    // 为该临时变量在栈上分配一个空间（逃逸）
    auto access =
        static_cast<frame::InFrameAccess *>(frame_->F_allocLocal(true));
    auto spilled_temp = node->NodeInfo();
    auto instr_end = assem_instr_->GetInstrList()->GetList().end();
    auto instr_it = assem_instr_->GetInstrList()->GetList().begin();
    // 按照指令遍历，对于src中的溢出变量重写从栈上取，dst中的溢出变量重写存入栈中
    for (; instr_it != instr_end; instr_it++) {
      temp::TempList *src = nullptr, *dst = nullptr;
      if (typeid(*(*instr_it)) == typeid(assem::MoveInstr)) {
        src = static_cast<assem::MoveInstr *>(*instr_it)->src_;
        dst = static_cast<assem::MoveInstr *>(*instr_it)->dst_;
      } else if (typeid(*(*instr_it)) == typeid(assem::OperInstr)) {
        src = static_cast<assem::OperInstr *>(*instr_it)->src_;
        dst = static_cast<assem::OperInstr *>(*instr_it)->dst_;
      } else {
        continue;
      }
      // 重写src中的溢出temp
      if (src && (src)->Contain(spilled_temp)) {
        // 替换溢出temp为新的从存储器取值的temp
        auto new_temp = temp::TempFactory::NewTemp();
        src = replaceTempList(src, spilled_temp, new_temp);
        if (typeid(*(*instr_it)) == typeid(assem::MoveInstr)) {
          static_cast<assem::MoveInstr *>(*instr_it)->src_ = src;
        } else if (typeid(*(*instr_it)) == typeid(assem::OperInstr)) {
          static_cast<assem::OperInstr *>(*instr_it)->src_ = src;
        }
        // 在该指令之前添加从存储器取地址的指令（源）
        std::ostringstream assem;
        assem << "movq " << frame_->name_->Name() << "_framesize"
              << access->offset << "(%rsp),`d0";
        assem_instr_->GetInstrList()->Insert(
            instr_it,
            new assem::OperInstr(assem.str(), new temp::TempList(new_temp),
                                 new temp::TempList(), nullptr));
      }
      // 重写src中的溢出temp
      if (dst && (dst)->Contain(spilled_temp)) {
        // 替换溢出temp为新的从存储器取值的temp
        auto new_temp = temp::TempFactory::NewTemp();
        dst = replaceTempList(dst, spilled_temp, new_temp);
        if (typeid(*(*instr_it)) == typeid(assem::MoveInstr)) {
          static_cast<assem::MoveInstr *>(*instr_it)->dst_ = dst;
        } else if (typeid(*(*instr_it)) == typeid(assem::OperInstr)) {
          static_cast<assem::OperInstr *>(*instr_it)->dst_ = dst;
        }
        // 在该指令之后添加从存储器取地址的指令（将新建的临时temp移入存储器中）
        std::ostringstream assem;
        assem << "movq `s0, " << frame_->name_->Name() << "_framesize"
              << access->offset << "(%rsp)";
        assem_instr_->GetInstrList()->Insert(
            std::next(instr_it),
            new assem::OperInstr(assem.str(), new temp::TempList(),
                                 new temp::TempList(new_temp), nullptr));
      }
    }
  }
}

temp::Map *RegAllocator::AssignRegisters() {
  // 建立一个map,将冲突图中的所有临时变量，按照染色情况，映射到颜色对应的寄存器名称
  auto reg_map = temp::Map::Empty();
  for (auto node : live_graph->interf_graph->Nodes()->GetList()) {
    reg_map->Enter(node->NodeInfo(), reg_manager->temp_map_->Look(color[node]));
  }
  return reg_map;
}

void RegAllocator::Reset() {
  delete live_graph_factory_;
  for (auto move_pair_it = movelist.begin(); move_pair_it != movelist.end();
       move_pair_it++) {
    delete move_pair_it->second;
  }
  delete simplifyWorklist;
  delete freezeWorklist;
  delete spillWorklist;
  delete spilledNodes;
  delete coalescedNodes;
  delete coloredNodes;
  delete selectStack;
  delete activeMoves;
  delete coalescedMoves;
  delete frozenMoves;
  delete constrainedMoves;
}

RegAllocator::~RegAllocator() { Reset(); }

} // namespace ra