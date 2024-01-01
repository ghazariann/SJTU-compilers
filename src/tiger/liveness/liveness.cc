#include "tiger/liveness/liveness.h"

#include <iostream>
#include <limits>

extern frame::RegManager *reg_manager;

namespace graph {

Node<temp::Temp> *IGraph::NewNode(temp::Temp *info) {
  // Make a new node in graph "g", with associated "info_"
  auto node = new Node<temp::Temp>();
  node->my_graph_ = this;
  node->my_key_ = nodecount_++;

  my_nodes_->Append(node);

  node->preds_ = new NodeList<temp::Temp>();
  node->succs_ = new NodeList<temp::Temp>();
  node->info_ = info;

  degree_[node] = 0;

  return node;
}

bool IGraph::IsAdjacent(Node<temp::Temp> *n, Node<temp::Temp> *m) {
  assert(n && m); // Ensure nodes are not null
  return adj_set_.find({n, m}) != adj_set_.end();
}

void IGraph::AddEdge(Node<temp::Temp> *from, Node<temp::Temp> *to) {
  assert(from && to);
  if (!IsAdjacent(from, to) && from != to) {
    adj_set_.emplace(from, to);
    adj_set_.emplace(to, from);

    // Add to adjacent list
    if (!precolored_->ContainsElement(from->NodeInfo())) {
      live::INodeList *singleTo = new live::INodeList(to);
      from->succs_ = from->succs_->Union(singleTo);
      from->IncrementIDegree();
    }
    if (!precolored_->ContainsElement(to->NodeInfo())) {
      live::INodeList *singleFrom = new live::INodeList(from);
      to->preds_ = to->preds_->Union(singleFrom);
      to->IncrementIDegree();
    }
  }
}

void IGraph::SetNodeDegree(Node<temp::Temp> *n, int d) {
  assert(n->my_graph_ == this);
  degree_.at(n) = d;
}

int IGraph::GetNodeDegree(Node<temp::Temp> *n) {
  assert(n->my_graph_ == this);
  return degree_.at(n);
}

void IGraph::ClearAllEdges() {
  adj_set_.clear();
  for (Node<temp::Temp> *n : my_nodes_->GetList()) {
    degree_.at(n) = 0;
    n->preds_->Clear();
    n->succs_->Clear();
  }
}

void IGraph::IncrementDegree(Node<temp::Temp> *n) {
  assert(n->my_graph_ == this);
  degree_.at(n)++;
}

void IGraph::DecrementDegree(Node<temp::Temp> *n) {
  assert(n->my_graph_ == this);
  degree_.at(n)--;
}

} // namespace graph

namespace temp {
bool TempList::ContainsElement(const Temp *element) const {
  for (auto t : temp_list_) {
    if (t == element)
      return true;
  }
  return false;
}

void TempList::AppendTempList(const TempList *tl) {
  // Append all elements of tl to this
  if (!tl || tl->temp_list_.empty())
    return;
  temp_list_.insert(temp_list_.end(), tl->temp_list_.begin(),
                    tl->temp_list_.end());
}

TempList *TempList::CreateUnionWithList(const TempList *tl) const {
  TempList *result = new TempList();
  result->AppendTempList(this);
  for (auto t : tl->GetList()) {
    if (!result->ContainsElement(t))
      result->Append(t);
  }
  return result;
}

TempList *TempList::CreateDifferenceWithList(const TempList *tl) const {
  TempList *result = new TempList();
  for (auto t : temp_list_) {
    if (!tl->ContainsElement(t))
      result->Append(t);
  }
  return result;
}

bool TempList::IsIdenticalToList(const TempList *tl) const {
  // Check if this and tl have the same elements
  TempList *differance1 = this->CreateDifferenceWithList(tl);
  TempList *differance2 = tl->CreateDifferenceWithList(this);
  return differance1->GetList().empty() && differance2->GetList().empty();
}

std::list<Temp *>::const_iterator
TempList::ReplaceElementAtIterator(std::list<Temp *>::const_iterator pos,
                                   Temp *temp) {
  // Replace element at pos with temp
  temp_list_.insert(pos, temp);
  pos = temp_list_.erase(pos);
  pos--;
  return pos;
}
} // namespace temp

namespace live {
LiveGraphFactory::LiveGraphFactory(fg::FGraphPtr flowgraph)
    : flowgraph_(flowgraph),
      live_graph_(new IGraph(reg_manager->Registers()), new MoveList()),
      in_(std::make_unique<graph::Table<assem::Instr, temp::TempList>>()),
      out_(std::make_unique<graph::Table<assem::Instr, temp::TempList>>()),
      temp_node_map_(new tab::Table<temp::Temp, INode>()),
      nodeInstractionMap(std::make_shared<NodeInstrMap>()) {}
bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
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
  for (auto move : move_list_)
    res->move_list_.push_back(move);
  for (auto move : list->GetList()) {
    if (!Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::MovesDifference(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    if (!list->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

void LiveGraphFactory::LiveMap() {

  for (fg::FNode *fnode : flowgraph_->Nodes()->GetList()) {
    in_.get()->Enter(fnode, new temp::TempList());
    out_.get()->Enter(fnode, new temp::TempList());
  }

  int finished = 0;
  int i = 0;

  while (finished != flowgraph_->nodecount_) {

    finished = 0;

    for (auto fnode_it = flowgraph_->Nodes()->GetList().rbegin();
         fnode_it != flowgraph_->Nodes()->GetList().rend(); fnode_it++) {
      // Compute out
      temp::TempList *out_n = new temp::TempList();
      for (fg::FNode *succ_fnode : (*fnode_it)->Succ()->GetList())
        out_n = out_n->CreateUnionWithList(in_.get()->Look(succ_fnode));

      // Compute in
      temp::TempList *in_n = (*fnode_it)->NodeInfo()->Use();
      in_n = in_n->CreateUnionWithList(
          out_n->CreateDifferenceWithList((*fnode_it)->NodeInfo()->Def()));

      if (out_n->IsIdenticalToList(out_.get()->Look(*fnode_it)) &&
          in_n->IsIdenticalToList(in_.get()->Look(*fnode_it))) {
        finished++;
      } else {
        out_.get()->Enter((*fnode_it), out_n);
        in_.get()->Enter(*fnode_it, in_n);
      }
    }
  }
}

void LiveGraphFactory::InterfGraph(MoveList **worklist_moves) {

  for (fg::FNode *fnode : flowgraph_->Nodes()->GetList()) {
    assem::Instr *instr = fnode->NodeInfo();
    temp::TempList *live = out_.get()->Look(fnode);

    if (typeid(*instr) == typeid(assem::MoveInstr)) { // move instruction
      assert(instr->Def()->GetList().size() == 1);
      assert(instr->Use()->GetList().size() == 1);

      live = live->CreateDifferenceWithList(instr->Use());

      temp::Temp *def_reg = instr->Def()->GetList().front();
      temp::Temp *use_reg = instr->Use()->GetList().front();
      INode *def_n = temp_node_map_->Look(def_reg);
      INode *use_n = temp_node_map_->Look(use_reg);
      MoveList *single_move =
          new MoveList(std::pair<INodePtr, INodePtr>(use_n, def_n));

      temp::TempList *defs_and_uses =
          instr->Def()->CreateUnionWithList(instr->Use());
      for (temp::Temp *reg : defs_and_uses->GetList()) {
        INode *n = temp_node_map_->Look(reg);
        MoveList *new_moves =
            live_graph_.move_list->Look(n)->Union(single_move);
        ;
        live_graph_.move_list->Set(n, new_moves);
      }

      // worklistMoves = (worklistMoves)->Union(single_move);
      *worklist_moves = (*worklist_moves)->Union(single_move);
    }

    live = live->CreateUnionWithList(instr->Def());

    // Add inteference edges
    temp::TempList *defs = instr->Def();
    for (temp::Temp *def_reg : defs->GetList()) {
      INode *def_n = temp_node_map_->Look(def_reg);
      for (temp::Temp *live_reg : live->GetList()) {
        INode *live_n = temp_node_map_->Look(live_reg);
        live_graph_.interf_graph->AddEdge(live_n, def_n);
      }
    }

    live = instr->Use()->CreateUnionWithList(
        live->CreateDifferenceWithList(instr->Def()));
  }
}

void LiveGraphFactory::Liveness(MoveList **worklist_moves) {
  LiveMap();
  InterfGraph(worklist_moves);
}

void LiveGraphFactory::BuildIGraph(assem::InstrList *instr_list) {
  // Add precolored registers as nodes to interference graph
  // Precolored registers will never be spilled
  for (temp::Temp *reg : reg_manager->Registers()->GetList()) {
    if (temp_node_map_->Look(reg) == nullptr) {
      INode *n = live_graph_.interf_graph->NewNode(reg);
      live_graph_.interf_graph->SetNodeDegree(n,
                                              std::numeric_limits<int>::max());
      live_graph_.move_list->Enter(n, new MoveList());
      temp_node_map_->Enter(reg, n);
      nodeInstractionMap.get()->insert(
          std::make_pair(n, new std::vector<InstrPos>()));
    }
  }

  // Add temporaries as nodes to interference graph
  for (auto instr_it = instr_list->GetList().cbegin();
       instr_it != instr_list->GetList().cend(); instr_it++) {
    INode *n;
    temp::TempList *defs_and_uses =
        (*instr_it)->Def()->CreateUnionWithList((*instr_it)->Use());
    for (temp::Temp *reg : defs_and_uses->GetList()) {
      if ((n = temp_node_map_->Look(reg)) == nullptr) {
        n = live_graph_.interf_graph->NewNode(reg);
        live_graph_.move_list->Enter(n, new MoveList());
        temp_node_map_->Enter(reg, n);
        nodeInstractionMap.get()->insert(
            std::make_pair(n, new std::vector<InstrPos>{instr_it}));
      } else {
        nodeInstractionMap.get()->at(n)->push_back(instr_it);
      }
    }
  }
}

} // namespace live