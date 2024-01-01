#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

#include <sstream>

extern frame::RegManager *reg_manager;
namespace ra {

RegAllocator::RegAllocator(frame::Frame *frame,
                           std::unique_ptr<cg::AssemInstr> assem_instr)
    : frame(frame), assemblyInstruction(std::move(assem_instr)) {

  globalMapping =
      temp::Map::LayerMap(reg_manager->temp_map_, temp::Map::Name());

  preColoredNodes = new live::INodeList();
  simplifyWorklist = new live::INodeList();
  freezeWorklist = new live::INodeList();
  spillWorklist = new live::INodeList();
  spilledNodes = new live::INodeList();
  coalescedNodes = new live::INodeList();
  coloredNodes = new live::INodeList();
  selectStack = new live::INodeList();

  coalescedMoves = new live::MoveList();
  constrainedMoves = new live::MoveList();
  frozenMoves = new live::MoveList();
  worklistMoves = new live::MoveList();
  activeMoves = new live::MoveList();
}

void RegAllocator::RegAlloc() {

  flowGraphFactory = std::make_unique<fg::FlowGraphFactory>(
      assemblyInstruction->GetInstrList());
  flowGraphFactory->AssemFlowGraph();

  liveGraphFactory = std::make_unique<live::LiveGraphFactory>(
      flowGraphFactory->GetFlowGraph());
  liveGraphFactory->BuildIGraph(assemblyInstruction->GetInstrList());

  liveGraphFactory->Liveness(&worklistMoves);

  InitializeNodeColors();
  InitializeNodeAliases();
  initialNodes = liveGraphFactory->GetLiveGraph().interf_graph->Nodes()->Diff(
      preColoredNodes);

  InitializeWorkLists();

  while (!IsWorklistEmpty()) {
    if (!simplifyWorklist->GetList().empty())
      Simplify();
    else if (!worklistMoves->GetList().empty())
      Coalesce();
    else if (!freezeWorklist->GetList().empty())
      Freeze();
    else if (!spillWorklist->GetList().empty())
      SelectNodeForSpilling();
  }

  AssignColorsToNodes();

  if (!spilledNodes->GetList().empty()) {
    RewriteProgram();
    RegAlloc();
  } else {
    RemoveRedundantMoves();
  }
}

bool RegAllocator::IsWorklistEmpty() {
  return simplifyWorklist->GetList().empty() &&
         worklistMoves->GetList().empty() &&
         freezeWorklist->GetList().empty() && spillWorklist->GetList().empty();
}

void RegAllocator::RemoveRedundantMoves() {
  assem::InstrList *instrList = assemblyInstruction->GetInstrList();
  std::vector<live::InstrPos> deleteMoves;
  for (auto instrIt = instrList->GetList().begin();
       instrIt != instrList->GetList().end(); ++instrIt) {
    if (typeid(**instrIt) == typeid(assem::MoveInstr)) {
      auto *moveInstr = static_cast<assem::MoveInstr *>(*instrIt);
      auto srcReg = moveInstr->src_->GetList().front();
      auto dstReg = moveInstr->dst_->GetList().front();
      auto srcNode = liveGraphFactory->GetTempNodeMap()->Look(srcReg);
      auto dstNode = liveGraphFactory->GetTempNodeMap()->Look(dstReg);
      if (colorMap[srcNode] == colorMap[dstNode])
        deleteMoves.push_back(instrIt);
    }
  }

  for (auto it : deleteMoves)
    instrList->Erase(it);
}

std::unique_ptr<Result> RegAllocator::BuildAllocationResult() {
  auto coloring = temp::Map::Empty();
  for (const auto &nodeColorPair : colorMap) {
    auto *reg = nodeColorPair.first->NodeInfo();
    int colorIndex = nodeColorPair.second;
    auto *regName =
        globalMapping->Look(reg_manager->Registers()->NthTemp(colorIndex));
    coloring->Enter(reg, regName);
  }
  auto result =
      std::make_unique<Result>(coloring, assemblyInstruction->GetInstrList());
  return result;
}

void RegAllocator::InitializeWorkLists() {
  for (live::INode *node : initialNodes->GetList()) {
    auto *singleNodeList = new live::INodeList(node);
    if (node->GetIDegree() >= reg_manager->RegisterCount()) {
      spillWorklist = spillWorklist->Union(singleNodeList);
    } else if (IsNodeMoveRelated(node)) {
      freezeWorklist = freezeWorklist->Union(singleNodeList);
    } else {
      simplifyWorklist = simplifyWorklist->Union(singleNodeList);
    }
  }
}

live::INodeList *RegAllocator::GetAdjacentNodes(live::INode *node) {
  return node->AdjacentList()->Diff(selectStack->Union(coalescedNodes));
}

live::MoveList *RegAllocator::GetNodeMoveList(live::INode *node) {
  auto *nodeMoves = liveGraphFactory->GetLiveGraph().move_list->Look(node);
  return nodeMoves->Intersect(activeMoves->Union(worklistMoves));
}

bool RegAllocator::IsNodeMoveRelated(live::INode *node) {
  auto *nodeMoves = GetNodeMoveList(node);
  return !nodeMoves->GetList().empty();
}

void RegAllocator::Simplify() {
  live::INode *node = simplifyWorklist->GetList().front();
  simplifyWorklist->DeleteNode(node);
  selectStack->Prepend(node);
  auto *adjacentNodes = GetAdjacentNodes(node);
  for (live::INode *adjNode : adjacentNodes->GetList()) {
    DecreaseNodeDegree(adjNode);
  }
}

void RegAllocator::DecreaseNodeDegree(live::INode *node) {
  if (preColoredNodes->Contain(node))
    return;
  int degree = node->GetIDegree();
  node->DecrementIDegree();
  if (degree == reg_manager->RegisterCount()) {
    auto *singleNodeList = new live::INodeList(node);
    EnableNodeMoves(singleNodeList->Union(GetAdjacentNodes(node)));
    spillWorklist = spillWorklist->Diff(singleNodeList);
    if (IsNodeMoveRelated(node))
      freezeWorklist = freezeWorklist->Union(singleNodeList);
    else
      simplifyWorklist = simplifyWorklist->Union(singleNodeList);
  }
}

void RegAllocator::EnableNodeMoves(live::INodeList *nodes) {
  for (live::INode *node : nodes->GetList()) {
    live::MoveList *nodeMoves = GetNodeMoveList(node);
    for (const auto &move : nodeMoves->GetList()) {
      if (activeMoves->Contain(move.first, move.second)) {
        auto *singleMove = new live::MoveList(move);
        activeMoves = activeMoves->MovesDifference(singleMove);
        worklistMoves = worklistMoves->Union(singleMove);
      }
    }
  }
}

void RegAllocator::Coalesce() {
  auto movePair = worklistMoves->GetList().front();
  auto *singleMove = new live::MoveList(movePair);
  live::INode *x = GetAlias(movePair.first);
  live::INode *y = GetAlias(movePair.second);
  live::INode *u, *v;

  if (IsPrecolored(y)) {
    u = y;
    v = x;
  } else {
    u = x;
    v = y;
  }

  worklistMoves = worklistMoves->MovesDifference(singleMove);

  if (u == v) {
    coalescedMoves = coalescedMoves->Union(singleMove);
    AddNodeToWorkList(u);
  } else if (IsPrecolored(v) || AreAdjacent(u, v)) {
    constrainedMoves = constrainedMoves->Union(singleMove);
    AddNodeToWorkList(u);
    AddNodeToWorkList(v);
  } else if (ApplyGeorgeHeuristic(u, v) || ApplyBriggsHeuristic(u, v)) {
    coalescedMoves = coalescedMoves->Union(singleMove);
    Combine(u, v);
    AddNodeToWorkList(u);
  } else {
    activeMoves = activeMoves->Union(singleMove);
  }
}

void RegAllocator::AddNodeToWorkList(live::INode *u) {
  if (!IsPrecolored(u) && !IsNodeMoveRelated(u) &&
      u->GetIDegree() < reg_manager->RegisterCount()) {
    auto *singleU = new live::INodeList(u);
    freezeWorklist = freezeWorklist->Diff(singleU);
    simplifyWorklist = simplifyWorklist->Union(singleU);
  }
}

bool RegAllocator::IsSimplifyCandidate(live::INode *t, live::INode *r) {
  return r->GetIDegree() < reg_manager->RegisterCount() || IsPrecolored(t) ||
         AreAdjacent(t, r);
}

bool RegAllocator::IsConservativeChoice(live::INodeList *nodes) {
  int k = 0;
  for (live::INode *n : nodes->GetList()) {
    if (n->GetIDegree() >= reg_manager->RegisterCount())
      k++;
  }
  return k < reg_manager->RegisterCount();
}

live::INode *RegAllocator::GetAlias(live::INode *n) {
  if (coalescedNodes->Contain(n))
    return GetAlias(aliasMap[n]);
  else
    return n;
}

void RegAllocator::Combine(live::INode *u, live::INode *v) {
  auto *singleV = new live::INodeList(v);

  if (freezeWorklist->Contain(v))
    freezeWorklist = freezeWorklist->Diff(singleV);
  else
    spillWorklist = spillWorklist->Diff(singleV);

  coalescedNodes = coalescedNodes->Union(singleV);
  aliasMap[v] = u;

  auto *uMoves = liveGraphFactory->GetLiveGraph().move_list->Look(u);
  auto *vMoves = liveGraphFactory->GetLiveGraph().move_list->Look(v);
  liveGraphFactory->GetLiveGraph().move_list->Enter(u, uMoves->Union(vMoves));
  EnableNodeMoves(singleV);

  auto *adjacentNodes = GetAdjacentNodes(v);
  for (live::INode *t : adjacentNodes->GetList()) {
    liveGraphFactory->GetLiveGraph().interf_graph->AddEdge(t, u);
    DecreaseNodeDegree(t);
  }

  if (u->GetIDegree() >= reg_manager->RegisterCount() &&
      freezeWorklist->Contain(u)) {
    auto *singleU = new live::INodeList(u);
    freezeWorklist = freezeWorklist->Diff(singleU);
    spillWorklist = spillWorklist->Union(singleU);
  }
}

bool RegAllocator::ApplyGeorgeHeuristic(live::INode *u, live::INode *v) {
  if (!IsPrecolored(u))
    return false;

  auto *adjacentNodes = GetAdjacentNodes(v);
  for (live::INode *t : adjacentNodes->GetList()) {
    if (!IsSimplifyCandidate(t, u))
      return false;
  }
  return true;
}

bool RegAllocator::ApplyBriggsHeuristic(live::INode *u, live::INode *v) {
  if (IsPrecolored(u))
    return false;

  auto *nodes = GetAdjacentNodes(u);
  nodes = nodes->Union(GetAdjacentNodes(v));
  return IsConservativeChoice(nodes);
}

bool RegAllocator::IsPrecolored(live::INode *n) {
  return preColoredNodes->Contain(n);
}

bool RegAllocator::AreAdjacent(live::INode *u, live::INode *v) {
  return liveGraphFactory->GetLiveGraph().interf_graph->IsAdjacent(u, v);
}

void RegAllocator::Freeze() {
  live::INode *u = freezeWorklist->GetList().front();
  auto *singleU = new live::INodeList(u);
  freezeWorklist = freezeWorklist->Diff(singleU);
  simplifyWorklist = simplifyWorklist->Union(singleU);
  FreezeMoves(u);
}

void RegAllocator::FreezeMoves(live::INode *u) {
  auto *uMoves = GetNodeMoveList(u);
  live::INode *v;

  for (const auto &movePair : uMoves->GetList()) {
    v = GetAlias(movePair.first == u ? movePair.second : movePair.first);

    auto *singleMove = new live::MoveList(movePair);
    activeMoves = activeMoves->MovesDifference(singleMove);
    frozenMoves = frozenMoves->Union(singleMove);

    if (GetNodeMoveList(v)->GetList().empty() &&
        v->GetIDegree() < reg_manager->RegisterCount()) {
      auto *singleV = new live::INodeList(v);
      freezeWorklist = freezeWorklist->Diff(singleV);
      simplifyWorklist = simplifyWorklist->Union(singleV);
    }
  }
}

void RegAllocator::SelectNodeForSpilling() {
  assert(!spillWorklist->GetList().empty());
  live::INode *m = SelectSpillCandidateHeuristically();

  auto *singleM = new live::INodeList(m);
  spillWorklist = spillWorklist->Diff(singleM);
  simplifyWorklist = simplifyWorklist->Union(singleM);
  FreezeMoves(m);
}

live::INode *RegAllocator::SelectSpillCandidateHeuristically() {
  live::INode *result = nullptr;
  int maxDistance = -1;
  assem::InstrList *instrList = assemblyInstruction->GetInstrList();

  for (live::INode *n : spillWorklist->GetList()) {
    int position = 0, start = -1, distance = -1;
    for (assem::Instr *instr : instrList->GetList()) {
      if (instr->Def()->ContainsElement(n->NodeInfo())) {
        start = position;
      }
      if (instr->Use()->ContainsElement(n->NodeInfo())) {
        distance = position - start;
        if (distance > maxDistance) {
          maxDistance = distance;
          result = n;
        }
      }
      position++;
    }

    if (distance == -1) {
      return n; // Node is defined but never used
    }
  }

  return result;
}

void RegAllocator::AssignColorsToNodes() {
  while (!selectStack->GetList().empty()) {
    live::INode *n = selectStack->GetList().front();
    selectStack->DeleteNode(n);

    std::set<int> availableColors;
    for (int c = 0; c < reg_manager->RegisterCount(); ++c) {
      availableColors.insert(c);
    }

    for (live::INode *w : n->AdjacentList()->GetList()) {
      live::INode *alias = GetAlias(w);
      if (coloredNodes->Union(preColoredNodes)->Contain(alias)) {
        availableColors.erase(colorMap[alias]);
      }
    }

    auto *singleN = new live::INodeList(n);
    if (availableColors.empty()) {
      spilledNodes = spilledNodes->Union(singleN);
    } else {
      coloredNodes = coloredNodes->Union(singleN);
      colorMap[n] = *availableColors.begin();
    }
  }

  for (live::INode *n : coalescedNodes->GetList()) {
    colorMap[n] = colorMap[GetAlias(n)];
  }
}

void RegAllocator::RewriteProgram() {
  auto *nodeInstrMap = liveGraphFactory->GetNodeInstrMap().get();

  for (live::INode *v : spilledNodes->GetList()) {
    frame::Access *acc = frame->AllocateLocal(true);
    std::string memPos = acc->ConsumeAccess(frame);

    auto nodeInstrs = nodeInstrMap->at(v);
    for (auto instrIt = nodeInstrs->begin(); instrIt != nodeInstrs->end();
         ++instrIt) {
      auto instrPos = *instrIt;
      assem::Instr *instr = *instrPos;
      temp::Temp *newReg = temp::TempFactory::NewTemp();

      // If the spilled temporary is used in the instruction
      if (instr->Use()->ContainsElement(v->NodeInfo())) {
        ReplaceInUseList(instr, v->NodeInfo(), newReg);

        // Insert a fetch instruction before the current instruction
        std::string fetchInstrStr = "movq " + memPos + ", `d0";
        auto *fetchInstr = new assem::OperInstr(
            fetchInstrStr, new temp::TempList(newReg),
            new temp::TempList(reg_manager->StackPointer()), nullptr);
        assemblyInstruction->GetInstrList()->Insert(instrPos, fetchInstr);
      }

      // If the spilled temporary is defined in the instruction
      if (instr->Def()->ContainsElement(v->NodeInfo())) {
        ReplaceInDefList(instr, v->NodeInfo(), newReg);

        // Insert a store instruction after the current instruction
        std::string storeInstrStr = "movq `s0, " + memPos;
        auto *storeInstr = new assem::OperInstr(
            storeInstrStr, nullptr,
            new temp::TempList({newReg, reg_manager->StackPointer()}), nullptr);
        assemblyInstruction->GetInstrList()->Insert(++instrPos, storeInstr);
      }
    }
  }

  // Clear all the lists and maps
  ClearAllListsAndMaps();
}

void RegAllocator::InitializeNodeColors() {
  auto tnMap = liveGraphFactory->GetTempNodeMap();
  int colorIndex = 0;
  for (temp::Temp *reg : reg_manager->Registers()->GetList()) {
    live::INode *node = tnMap->Look(reg);
    preColoredNodes->Append(node);
    colorMap[node] = colorIndex++;
  }
}

void RegAllocator::InitializeNodeAliases() {
  auto *allNodes = liveGraphFactory->GetLiveGraph().interf_graph->Nodes();
  for (live::INode *n : allNodes->GetList()) {
    aliasMap[n] = n;
  }
}
void RegAllocator::ReplaceInUseList(assem::Instr *instr, temp::Temp *oldReg,
                                    temp::Temp *newReg) {
  for (auto tempIt = instr->Use()->GetList().begin();
       tempIt != instr->Use()->GetList().end(); tempIt++) {
    if (*tempIt == oldReg) {
      instr->Use()->ReplaceElementAtIterator(tempIt, newReg);
      break;
    }
  }
}

void RegAllocator::ReplaceInDefList(assem::Instr *instr, temp::Temp *oldReg,
                                    temp::Temp *newReg) {
  for (auto tempIt = instr->Def()->GetList().begin();
       tempIt != instr->Def()->GetList().end(); tempIt++) {
    if (*tempIt == oldReg) {
      instr->Def()->ReplaceElementAtIterator(tempIt, newReg);
      break;
    }
  }
}

void RegAllocator::ClearAllListsAndMaps() {
  spilledNodes->Clear();
  coloredNodes->Clear();
  coalescedNodes->Clear();
  coalescedMoves->Clear();
  constrainedMoves->Clear();
  frozenMoves->Clear();
  worklistMoves->Clear();
  activeMoves->Clear();
  colorMap.clear();
  aliasMap.clear();

  flowGraphFactory = nullptr;
  liveGraphFactory = nullptr;
}
void RegAllocator::PrintMovePairList() {
  std::cout << "worklist_moves_: ";
  PrintMovePairList(worklistMoves);

  std::cout << "coalesced_moves_: ";
  PrintMovePairList(coalescedMoves);

  std::cout << "constrained_moves_: ";
  PrintMovePairList(constrainedMoves);

  std::cout << "frozen_moves_: ";
  PrintMovePairList(frozenMoves);

  std::cout << "active_moves_: ";
  PrintMovePairList(activeMoves);
}

void RegAllocator::PrintNodeAliases() {
  std::cout << "PrintAlias: ";
  for (auto node :
       liveGraphFactory->GetLiveGraph().interf_graph->Nodes()->GetList()) {
    std::cout << *globalMapping->Look(node->NodeInfo()) << '-'
              << *globalMapping->Look(aliasMap[node]->NodeInfo()) << ' ';
  }
  std::cout << std::endl;
}

void RegAllocator::PrintNodeList() {
  std::cout << "spilled_nodes_: ";
  PrintNodeListContent(spilledNodes);

  std::cout << "coalesced_nodes_: ";
  PrintNodeListContent(coalescedNodes);

  std::cout << "colored_nodes_: ";
  PrintNodeListContent(coloredNodes);

  std::cout << "select_stack_: ";
  PrintNodeListContent(selectStack);
}

void RegAllocator::PrintMovePairList(const live::MoveList *moveList) {
  for (const auto &movePair : moveList->GetList()) {
    std::cout << *globalMapping->Look(movePair.first->NodeInfo()) << "->"
              << *globalMapping->Look(movePair.second->NodeInfo()) << " ";
  }
  std::cout << std::endl;
}

void RegAllocator::PrintNodeListContent(const live::INodeList *nodeList) {
  for (const auto &node : nodeList->GetList()) {
    std::cout << *globalMapping->Look(node->NodeInfo()) << ' ';
  }
  std::cout << std::endl;
}

} // namespace ra