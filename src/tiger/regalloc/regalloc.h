#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"
#include <map>

namespace ra {

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result() {
    delete coloring_;
    delete il_;
  };
};

class RegAllocator {
public:
  RegAllocator(frame::Frame *frame,
               std::unique_ptr<cg::AssemInstr> assem_instr);

  void RegAlloc();
  std::unique_ptr<Result> BuildAllocationResult();

private:
  frame::Frame *frame;
  std::unique_ptr<cg::AssemInstr> assemblyInstruction;
  temp::Map *globalMapping;

  live::INodeList *spillWorklist;
  live::INodeList *spilledNodes;
  live::INodeList *preColoredNodes;
  live::INodeList *simplifyWorklist;
  live::INodeList *freezeWorklist;

  live::INodeList *coalescedNodes;
  live::INodeList *coloredNodes;
  live::INodeList *selectStack;

  live::MoveList *coalescedMoves;
  live::MoveList *constrainedMoves;
  live::MoveList *frozenMoves;
  live::MoveList *worklistMoves;
  live::MoveList *activeMoves;

  std::map<live::INode *, int> colorMap;
  std::unordered_map<live::INode *, live::INode *> aliasMap;

  std::unique_ptr<fg::FlowGraphFactory> flowGraphFactory;
  std::unique_ptr<live::LiveGraphFactory> liveGraphFactory;
  live::INodeList *initialNodes;

  bool IsWorklistEmpty();
  void RemoveRedundantMoves();

  void InitializeNodeColors();
  void InitializeNodeAliases();

  void InitializeWorkLists();
  live::INodeList *GetAdjacentNodes(live::INode *n);
  live::MoveList *GetNodeMoveList(live::INode *n);
  bool IsNodeMoveRelated(live::INode *n);

  void Simplify();
  void DecreaseNodeDegree(live::INode *n);
  void EnableNodeMoves(live::INodeList *nodes);

  void Coalesce();
  void AddNodeToWorkList(live::INode *u);
  bool IsSimplifyCandidate(live::INode *t, live::INode *r);
  bool IsConservativeChoice(live::INodeList *nodes);
  live::INode *GetAlias(live::INode *n);
  void Combine(live::INode *u, live::INode *v);
  bool ApplyGeorgeHeuristic(live::INode *u, live::INode *v);
  bool ApplyBriggsHeuristic(live::INode *u, live::INode *v);
  bool IsPrecolored(live::INode *n);
  bool AreAdjacent(live::INode *u, live::INode *v);

  void Freeze();
  void FreezeMoves(live::INode *u);

  void SelectNodeForSpilling();
  live::INode *SelectSpillCandidateHeuristically();
  void AssignColorsToNodes();
  void RewriteProgram();

  void PrintMovePairList();
  void PrintNodeAliases();
  void PrintNodeList();

  void ReplaceInUseList(assem::Instr *instr, temp::Temp *oldReg,
                        temp::Temp *newReg);
  void ReplaceInDefList(assem::Instr *instr, temp::Temp *oldReg,
                        temp::Temp *newReg);
  void ClearAllListsAndMaps();
  void PrintMovePairList(const live::MoveList *moveList);
  void PrintNodeListContent(const live::INodeList *nodeList);
};

} // namespace ra

#endif