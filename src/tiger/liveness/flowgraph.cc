#include "tiger/liveness/flowgraph.h"

namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  ConstructGraphFromInstructions(instr_list_);
  AddGraphEdges();
}

void FlowGraphFactory::ConstructGraphFromInstructions(
    assem::InstrList *instructions) {
  for (auto *instruction : instructions->GetList()) {
    auto *node = flowgraph_->NewNode(instruction);
    HandleLabelInstruction(instruction, node);
  }
}

void FlowGraphFactory::HandleLabelInstruction(assem::Instr *instruction,
                                              FNode *node) {
  if (typeid(*instruction) == typeid(assem::LabelInstr)) {
    auto *labelInstruction = static_cast<assem::LabelInstr *>(instruction);
    label_map_.get()->Enter(labelInstruction->label_, node);
  }
}

void FlowGraphFactory::AddGraphEdges() {
  for (std::list<FNode *>::const_iterator nodeIterator =
           flowgraph_->Nodes()->GetList().begin();
       nodeIterator != std::prev(flowgraph_->Nodes()->GetList().end());
       ++nodeIterator) {
    auto *node = *nodeIterator;
    auto *instruction = node->NodeInfo();

    if (typeid(*instruction) == typeid(assem::OperInstr)) {
      assem::OperInstr *operationInstruction =
          static_cast<assem::OperInstr *>(instruction);
      if (operationInstruction->jumps_) {
        for (auto *label : *operationInstruction->jumps_->labels_) {
          flowgraph_->AddEdge(*nodeIterator, label_map_.get()->Look(label));
        }

        if (operationInstruction->assem_.find("jmp") == std::string::npos) {
          flowgraph_->AddEdge(*nodeIterator, *std::next(nodeIterator));
        }
      } else {
        flowgraph_->AddEdge(*nodeIterator, *std::next(nodeIterator));
      }
    } else {
      flowgraph_->AddEdge(node, *std::next(nodeIterator));
    }
  }
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const { return new temp::TempList(); }

temp::TempList *MoveInstr::Def() const {
  return dst_ == nullptr ? new temp::TempList() : dst_;
}

temp::TempList *OperInstr::Def() const {
  return dst_ == nullptr ? new temp::TempList() : dst_;
}

temp::TempList *LabelInstr::Use() const { return new temp::TempList(); }

temp::TempList *MoveInstr::Use() const {
  return src_ == nullptr ? new temp::TempList() : src_;
}

temp::TempList *OperInstr::Use() const {
  return src_ == nullptr ? new temp::TempList() : src_;
}
} // namespace assem
