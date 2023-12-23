#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;
constexpr int wordsize = 8;

} // namespace

namespace cg {

void CodeGen::Codegen() {
  /* TODO: Put your lab5 code here */
  tree::StmList *stm_list = traces_.get()->GetStmList();
  assem::InstrList *instr_list = new assem::InstrList();
  for (auto stm : stm_list->GetList())
    stm->Munch(*instr_list, fs_);
  assem_instr_ = std::make_unique<AssemInstr>(instr_list);
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {
/* TODO: Put your lab5 code here */
static assem::MemFetch *MunchMem(tree::Exp *memExp, bool isDestination,
                                 assem::InstrList &instrList,
                                 std::string_view frameSpecific) {
  tree::Exp *innerExp = static_cast<tree::MemExp *>(memExp)->exp_;
  std::stringstream memStream;

  if (typeid(*innerExp) == typeid(tree::BinopExp) &&
      static_cast<tree::BinopExp *>(innerExp)->op_ == tree::PLUS_OP) {
    tree::BinopExp *binExp = static_cast<tree::BinopExp *>(innerExp);

    auto handleConstantOffset = [&](tree::Exp *offsetExp, tree::Exp *otherExp) {
      tree::ConstExp *constExp = static_cast<tree::ConstExp *>(offsetExp);
      temp::Temp *baseReg = otherExp->Munch(instrList, frameSpecific);
      memStream << (constExp->consti_ == 0 ? ""
                                           : std::to_string(constExp->consti_))
                << (isDestination ? "(`d0)" : "(`s0)");
      return new assem::MemFetch(memStream.str(), new temp::TempList(baseReg));
    };

    if (typeid(*binExp->right_) == typeid(tree::ConstExp)) {
      return handleConstantOffset(binExp->right_, binExp->left_);
    } else if (typeid(*binExp->left_) == typeid(tree::ConstExp)) {
      return handleConstantOffset(binExp->left_, binExp->right_);
    } else {
      temp::Temp *memReg = innerExp->Munch(instrList, frameSpecific);
      memStream << (isDestination ? "(`d0)" : "(`s0)");
      return new assem::MemFetch(memStream.str(), new temp::TempList(memReg));
    }
  } else {
    temp::Temp *memReg = innerExp->Munch(instrList, frameSpecific);
    memStream << (isDestination ? "(`d0)" : "(`s0)");
    return new assem::MemFetch(memStream.str(), new temp::TempList(memReg));
  }
}

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::LabelInstr(label_->Name(), label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  std::string instr_str = "jmp " + exp_->name_->Name();
  instr_list.Append(new assem::OperInstr(instr_str, nullptr, nullptr,
                                         new assem::Targets(jumps_)));
}

void CjumpStm::Munch(assem::InstrList &instrList,
                     std::string_view frameSpecific) {
  std::stringstream instrStream;

  if (typeid(*right_) == typeid(tree::ConstExp)) {
    tree::ConstExp *rightConst = static_cast<tree::ConstExp *>(right_);
    temp::Temp *leftReg = left_->Munch(instrList, frameSpecific);
    instrStream << "cmpq $" << rightConst->consti_ << ", `s0";
    instrList.Append(new assem::OperInstr(
        instrStream.str(), nullptr, new temp::TempList(leftReg), nullptr));
  } else {
    temp::Temp *leftReg = left_->Munch(instrList, frameSpecific);
    temp::Temp *rightReg = right_->Munch(instrList, frameSpecific);
    instrList.Append(
        new assem::OperInstr("cmpq `s0, `s1", nullptr,
                             new temp::TempList({rightReg, leftReg}), nullptr));
  }

  instrStream.str(""); // Clearing the stream

  switch (op_) {
  case EQ_OP:
    instrStream << "je " << true_label_->Name();
    break;
  case NE_OP:
    instrStream << "jne " << true_label_->Name();
    break;
  case LT_OP:
    instrStream << "jl " << true_label_->Name();
    break;
  case GT_OP:
    instrStream << "jg " << true_label_->Name();
    break;
  case LE_OP:
    instrStream << "jle " << true_label_->Name();
    break;
  case GE_OP:
    instrStream << "jge " << true_label_->Name();
    break;
  default:
    return; // Error handling
  }
  instrList.Append(new assem::OperInstr(
      instrStream.str(), nullptr, nullptr,
      new assem::Targets(new std::vector<temp::Label *>{true_label_})));
}

void MoveStm::Munch(assem::InstrList &instrList,
                    std::string_view frameSpecific) {
  std::stringstream instrStream;

  if (typeid(*dst_) == typeid(tree::MemExp)) {
    temp::Temp *srcReg = src_->Munch(instrList, frameSpecific);
    assem::MemFetch *memFetch = MunchMem(dst_, true, instrList, frameSpecific);
    instrStream << "movq `s0, " << memFetch->fetch_;
    instrList.Append(new assem::MoveInstr(instrStream.str(), memFetch->regs_,
                                          new temp::TempList(srcReg)));
  } else {
    if (typeid(*src_) == typeid(tree::MemExp)) {
      assem::MemFetch *memFetch =
          MunchMem(src_, false, instrList, frameSpecific);
      temp::Temp *dstReg = dst_->Munch(instrList, frameSpecific);
      instrStream << "movq " << memFetch->fetch_ << ", `d0";
      instrList.Append(new assem::MoveInstr(
          instrStream.str(), new temp::TempList(dstReg), memFetch->regs_));
    } else if (typeid(*src_) == typeid(tree::ConstExp)) {
      temp::Temp *dstReg = dst_->Munch(instrList, frameSpecific);
      instrStream << "movq $" << static_cast<tree::ConstExp *>(src_)->consti_
                  << ", `d0";
      instrList.Append(new assem::MoveInstr(
          instrStream.str(), new temp::TempList(dstReg), nullptr));
    } else {
      temp::Temp *srcReg = src_->Munch(instrList, frameSpecific);
      temp::Temp *dstReg = dst_->Munch(instrList, frameSpecific);
      instrList.Append(new assem::MoveInstr("movq `s0, `d0",
                                            new temp::TempList(dstReg),
                                            new temp::TempList(srcReg)));
    }
  }
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  exp_->Munch(instr_list, fs);
}
void LoadOperand(tree::Exp *operand, temp::Temp *targetReg,
                 assem::InstrList &instrList, std::string_view frameSpecific) {
  std::stringstream instrStream;
  // Loading different types of operands into the target register
  if (typeid(*operand) == typeid(tree::ConstExp)) {
    tree::ConstExp *constOperand = static_cast<tree::ConstExp *>(operand);
    instrStream << "movq $" << constOperand->consti_ << ", `d0";
    instrList.Append(new assem::OperInstr(
        instrStream.str(), new temp::TempList(targetReg), nullptr, nullptr));
  } else if (typeid(*operand) == typeid(tree::MemExp)) {
    assem::MemFetch *fetch = MunchMem(operand, false, instrList, frameSpecific);
    instrStream << "movq " << fetch->fetch_ << ", `d0";
    instrList.Append(new assem::MoveInstr(
        instrStream.str(), new temp::TempList(targetReg), fetch->regs_));
  } else {
    temp::Temp *operandReg = operand->Munch(instrList, frameSpecific);
    instrList.Append(new assem::MoveInstr("movq `s0, `d0",
                                          new temp::TempList(targetReg),
                                          new temp::TempList(operandReg)));
  }
}

void PerformOperation(tree::Exp *operand, const std::string &assemblyInstr,
                      temp::Temp *rax, temp::Temp *rdx,
                      assem::InstrList &instrList,
                      std::string_view frameSpecific) {
  std::stringstream instrStream;
  if (typeid(*operand) == typeid(tree::ConstExp)) {
    tree::ConstExp *constOperand = static_cast<tree::ConstExp *>(operand);
    instrStream << assemblyInstr << " $" << constOperand->consti_;
    instrList.Append(
        new assem::OperInstr(instrStream.str(), new temp::TempList({rdx, rax}),
                             new temp::TempList({rdx, rax}), nullptr));
  } else {
    temp::Temp *operandReg = operand->Munch(instrList, frameSpecific);
    instrStream << assemblyInstr << " `s2";
    instrList.Append(new assem::OperInstr(
        instrStream.str(), new temp::TempList({rdx, rax}),
        new temp::TempList({rdx, rax, operandReg}), nullptr));
  }
}
temp::Temp *BinopExp::Munch(assem::InstrList &instrList,
                            std::string_view frameSpecific) {
  std::string assemblyInstr;
  std::stringstream instrStream;

  // Handle special case with frame pointer
  if (op_ == PLUS_OP && typeid(*right_) == typeid(tree::NameExp) &&
      static_cast<tree::NameExp *>(right_)->name_->Name() == frameSpecific) {
    temp::Temp *leftReg = left_->Munch(instrList, frameSpecific);
    temp::Temp *resultReg = temp::TempFactory::NewTemp();
    instrStream << "leaq " << frameSpecific << "(`s0), `d0";
    instrList.Append(
        new assem::OperInstr(instrStream.str(), new temp::TempList(resultReg),
                             new temp::TempList(leftReg), nullptr));
    return resultReg;
  }

  // Handling addition and subtraction
  if (op_ == PLUS_OP || op_ == MINUS_OP) {
    assemblyInstr = (op_ == PLUS_OP) ? "addq" : "subq";
    temp::Temp *leftReg = left_->Munch(instrList, frameSpecific);
    temp::Temp *resultReg = temp::TempFactory::NewTemp();

    instrList.Append(new assem::MoveInstr("movq `s0, `d0",
                                          new temp::TempList(resultReg),
                                          new temp::TempList(leftReg)));

    if (typeid(*right_) == typeid(tree::ConstExp)) { // Immediate operand
      tree::ConstExp *rightConst = static_cast<tree::ConstExp *>(right_);
      instrStream << assemblyInstr << " $" << rightConst->consti_ << ", `d0";
      instrList.Append(
          new assem::OperInstr(instrStream.str(), new temp::TempList(resultReg),
                               new temp::TempList(resultReg), nullptr));
      return resultReg;
    } else { // Register operand
      temp::Temp *rightReg = right_->Munch(instrList, frameSpecific);
      instrStream << assemblyInstr << " `s1, `d0";
      instrList.Append(new assem::OperInstr(
          instrStream.str(), new temp::TempList(resultReg),
          new temp::TempList({resultReg, rightReg}), nullptr));
      return resultReg;
    }
  }

  // Handling multiplication and division
  if (op_ == MUL_OP || op_ == DIV_OP) {
    assemblyInstr = (op_ == MUL_OP) ? "imulq" : "idivq";
    temp::Temp *rax = reg_manager->ReturnValue();
    temp::Temp *rdx = reg_manager->GetArithmeticRegister();
    temp::Temp *raxSaver = temp::TempFactory::NewTemp();
    temp::Temp *rdxSaver = temp::TempFactory::NewTemp();

    // Save rax and rdx
    instrList.Append(new assem::MoveInstr("movq `s0, `d0",
                                          new temp::TempList(raxSaver),
                                          new temp::TempList(rax)));
    instrList.Append(new assem::MoveInstr("movq `s0, `d0",
                                          new temp::TempList(rdxSaver),
                                          new temp::TempList(rdx)));

    // Load the left operand into rax
    LoadOperand(left_, rax, instrList, frameSpecific);

    // Convert quadword to octaword if dividing
    if (op_ == DIV_OP) {
      instrList.Append(new assem::OperInstr("cqto",
                                            new temp::TempList({rdx, rax}),
                                            new temp::TempList(rax), nullptr));
    }

    // Perform the operation
    PerformOperation(right_, assemblyInstr, rax, rdx, instrList, frameSpecific);

    // Move the result to a new register
    temp::Temp *resultReg = temp::TempFactory::NewTemp();
    instrList.Append(new assem::MoveInstr("movq `s0, `d0",
                                          new temp::TempList(resultReg),
                                          new temp::TempList(rax)));

    // Restore rax and rdx
    instrList.Append(new assem::MoveInstr("movq `s0, `d0",
                                          new temp::TempList(rax),
                                          new temp::TempList(raxSaver)));
    instrList.Append(new assem::MoveInstr("movq `s0, `d0",
                                          new temp::TempList(rdx),
                                          new temp::TempList(rdxSaver)));

    return resultReg;
  }

  return temp::TempFactory::NewTemp(); // Error handling if the operation is not
                                       // supported
}

temp::Temp *MemExp::Munch(assem::InstrList &instrList,
                          std::string_view frameSpecific) {
  temp::Temp *registerTemp = temp::TempFactory::NewTemp();
  assem::MemFetch *memFetch = MunchMem(this, false, instrList, frameSpecific);
  std::stringstream instrStream;
  instrStream << "movq " << memFetch->fetch_ << ", `d0";
  instrList.Append(new assem::MoveInstr(
      instrStream.str(), new temp::TempList(registerTemp), memFetch->regs_));
  return registerTemp;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  return temp_;
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  stm_->Munch(instr_list, fs);
  return exp_->Munch(instr_list, fs);
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *reg = temp::TempFactory::NewTemp();
  std::stringstream instr_ss;
  // load address
  instr_ss << "leaq " << name_->Name() << "("
           << *reg_manager->temp_map_->Look(reg_manager->ProgramCounter())
           << "), `d0";
  assem::Instr *instr =
      new assem::MoveInstr(instr_ss.str(), new temp::TempList(reg), nullptr);
  instr_list.Append(instr);
  return reg;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *reg = temp::TempFactory::NewTemp();
  std::stringstream instr_ss;
  instr_ss << "movq $" << consti_ << ", `d0";
  assem::Instr *instr =
      new assem::MoveInstr(instr_ss.str(), new temp::TempList(reg), nullptr);
  instr_list.Append(instr);
  return reg;
}

temp::Temp *CallExp::Munch(assem::InstrList &instrList,
                           std::string_view frameSpecific) {
  temp::Temp *rax = reg_manager->ReturnValue();
  std::stringstream instrStream;

  if (typeid(*fun_) != typeid(tree::NameExp)) // Error handling
    return rax;

  temp::TempList *argList = args_->MunchArgs(instrList, frameSpecific);
  instrStream << "callq " << static_cast<tree::NameExp *>(fun_)->name_->Name();
  instrList.Append(new assem::OperInstr(
      instrStream.str(), reg_manager->CallerSaves(), argList, nullptr));
  return rax;
}
void ProcessArgument(tree::Exp *arg, temp::Temp *dstReg,
                     assem::InstrList &instrList,
                     std::string_view frameSpecific,
                     std::stringstream &instrStream) {
  if (typeid(*arg) == typeid(tree::ConstExp)) {
    tree::ConstExp *constExp = static_cast<tree::ConstExp *>(arg);
    instrStream << "movq $" << constExp->consti_ << ", `d0";
    instrList.Append(new assem::OperInstr(
        instrStream.str(), new temp::TempList(dstReg), nullptr, nullptr));
  } else {
    temp::Temp *srcReg = arg->Munch(instrList, frameSpecific);
    instrStream << "movq `s0, `d0";
    instrList.Append(new assem::MoveInstr(instrStream.str(),
                                          new temp::TempList(dstReg),
                                          new temp::TempList(srcReg)));
  }
}
void HandleStackArguments(tree::Exp *arg, int index, int argRegCount,
                          assem::InstrList &instrList,
                          std::string_view frameSpecific,
                          std::stringstream &instrStream) {
  int stackOffset = (index - argRegCount) * wordsize;
  if (typeid(*arg) == typeid(tree::ConstExp)) {
    tree::ConstExp *constExp = static_cast<tree::ConstExp *>(arg);
    instrStream << "movq $" << constExp->consti_ << ", " << stackOffset << "("
                << *reg_manager->temp_map_->Look(reg_manager->StackPointer())
                << ")";
    instrList.Append(new assem::OperInstr(
        instrStream.str(), new temp::TempList(reg_manager->StackPointer()),
        nullptr, nullptr));
  } else {
    temp::Temp *srcReg = arg->Munch(instrList, frameSpecific);
    instrStream << "movq `s0, " << stackOffset << "("
                << *reg_manager->temp_map_->Look(reg_manager->StackPointer())
                << ")";
    instrList.Append(new assem::OperInstr(
        instrStream.str(), new temp::TempList(reg_manager->StackPointer()),
        new temp::TempList(srcReg), nullptr));
  }
}
temp::TempList *ExpList::MunchArgs(assem::InstrList &instrList,
                                   std::string_view frameSpecific) {
  temp::TempList *argList = new temp::TempList();
  std::stringstream instrStream;
  int argRegCount = reg_manager->ArgRegs()->GetList().size();
  int index = 0;

  for (tree::Exp *arg : this->GetList()) {
    if (index < argRegCount) {
      temp::Temp *dstReg = reg_manager->ArgRegs()->NthTemp(index);
      ProcessArgument(arg, dstReg, instrList, frameSpecific, instrStream);
      argList->Append(dstReg);
    } else {
      HandleStackArguments(arg, index, argRegCount, instrList, frameSpecific,
                           instrStream);
    }
    instrStream.str(""); // Clearing the stream
    ++index;
  }

  if (index > argRegCount)
    argList->Append(reg_manager->StackPointer());

  return argList;
}

} // namespace tree
