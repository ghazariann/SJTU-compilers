#include "tiger/frame/x64frame.h"
#include "tiger/codegen/assem.h"
#include <iostream>
#include <sstream>
extern frame::RegManager *reg_manager;

namespace frame {
/* TODO: Put your lab5 code here */
std::unordered_map<X64RegManager::Register, std::string>
    X64RegManager::registerMap =
        std::unordered_map<X64RegManager::Register, std::string>{
            {RBP, "%rbp"}, {RSP, "%rsp"}, {RAX, "%rax"}, {RBX, "%rbx"},
            {RCX, "%rcx"}, {RDX, "%rdx"}, {RSI, "%rsi"}, {RDI, "%rdi"},
            {R8, "%r8"},   {R9, "%r9"},   {R10, "%r10"}, {R11, "%r11"},
            {R12, "%r12"}, {R13, "%r13"}, {R14, "%r14"}, {R15, "%r15"}};

X64RegManager::X64RegManager() {
  //
  for (int i = 0; i < REG_COUNT; ++i) {
    temp::Temp *newRegister = temp::TempFactory::NewTemp();
    regs_.push_back(newRegister);
    temp_map_->Enter(newRegister, &registerMap.at(static_cast<Register>(i)));
  }
}

temp::TempList *X64RegManager::Registers() {
  temp::TempList *registers = new temp::TempList();
  for (int r = 0; r < REG_COUNT; r++) {
    registers->Append(regs_.at(r));
  }
  return registers;
}

temp::TempList *X64RegManager::ArgRegs() {
  temp::TempList *argRegisters = new temp::TempList({
      regs_.at(RDI),
      regs_.at(RSI),
      regs_.at(RDX),
      regs_.at(RCX),
      regs_.at(R8),
      regs_.at(R9),
  });
  return argRegisters;
}

temp::TempList *X64RegManager::CallerSaves() {
  temp::TempList *CallerSavesRegisters = new temp::TempList(
      {regs_.at(RAX), regs_.at(R8), regs_.at(R9), regs_.at(R10), regs_.at(R11),
       regs_.at(RDI), regs_.at(RSI), regs_.at(RDX), regs_.at(RCX)});
  return CallerSavesRegisters;
}

temp::TempList *X64RegManager::CalleeSaves() {
  temp::TempList *calleeSavesRegisters = new temp::TempList({
      regs_.at(RBX),
      regs_.at(RBP),
      regs_.at(R12),
      regs_.at(R13),
      regs_.at(R14),
      regs_.at(R15),
  });
  return calleeSavesRegisters;
}

temp::TempList *X64RegManager::ReturnSink() {
  temp::TempList *temps = CalleeSaves();
  temps->Append(regs_.at(RAX));
  temps->Append(regs_.at(RSP));
  return temps;
}

temp::Temp *X64RegManager::FramePointer() { return regs_.at(RBP); }

temp::Temp *X64RegManager::StackPointer() { return regs_.at(RSP); }

temp::Temp *X64RegManager::ReturnValue() { return regs_.at(RAX); }

int X64RegManager::WordSize() { return WORD_SIZE; }
int X64RegManager::RegisterCount() { return REG_COUNT; }
temp::Temp *X64RegManager::GetArithmeticRegister() { return regs_.at(RDX); }

class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}
  /* TODO: Put your lab5 code here */
  std::string ConsumeAccess(Frame *frame) override {
    std::stringstream ss;
    ss << "(" << frame->frameSizeLabel_->Name() << "-" << offset << ")("
       << *reg_manager->temp_map_->Look(reg_manager->StackPointer()) << ")";
    return ss.str();
  }
};

class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  /* TODO: Put your lab5 code here */
  std::string ConsumeAccess(Frame *frame) override {
    return *temp::Map::Name()->Look(reg);
  }
};

class X64Frame : public Frame {
  /* TODO: Put your lab5 code here */
public:
  X64Frame(temp::Label *name) : Frame(name) { wordSize_ = WORD_SIZE_X64_; }
  int GetWordSize() const override;
  Access *AllocateLocal(bool escape) override;
  tree::Exp *GetFrameAddress() const override;
  std::string GetFrameLabel() const override;
  tree::Exp *GetStackOffset(int frame_offset) const override;
};
/* TODO: Put your lab5 code here */
Access *X64Frame::AllocateLocal(bool escape) {
  Access *access;
  if (escape) {
    localVariableCount_++;
    access = new InFrameAccess(localVariableCount_ * wordSize_);
  } else {
    access = new InRegAccess(temp::TempFactory::NewTemp());
  }
  localAccesses_.push_back(access);
  return access;
}

tree::Exp *X64Frame::GetFrameAddress() const {
  return new tree::BinopExp(tree::PLUS_OP,
                            new tree::TempExp(reg_manager->StackPointer()),
                            new tree::NameExp(frameSizeLabel_));
}

int X64Frame::GetWordSize() const { return wordSize_; }

std::string X64Frame::GetFrameLabel() const { return frameLabel_->Name(); }

tree::Exp *X64Frame::GetStackOffset(int frame_offset) const {
  // offset is calculated from frame pointer (rbp) and frame size label
  return new tree::BinopExp(tree::MINUS_OP, new tree::NameExp(frameSizeLabel_),
                            new tree::ConstExp(frame_offset));
}

Frame *NewFrame(temp::Label *name, std::vector<bool> formals) {
  Frame *_frame = new X64Frame(name);
  int frameOffset = _frame->GetWordSize();
  _frame->frameSizeLabel_ =
      temp::LabelFactory::NamedLabel(name->Name() + "_framesize");

  tree::TempExp *framePointerExpression =
      new tree::TempExp(temp::TempFactory::NewTemp());
  _frame->viewShiftStatement =
      new tree::MoveStm(framePointerExpression, _frame->GetFrameAddress());

  tree::Exp *destinationExppression;
  tree::Stm *singleViewShift;
  int ArgRegCount = reg_manager->ArgRegs()->GetList().size();
  tree::Exp *framePointerCopy;

  if (formals.size() > ArgRegCount) {
    framePointerCopy = new tree::TempExp(temp::TempFactory::NewTemp());
    _frame->viewShiftStatement = new tree::SeqStm(
        _frame->viewShiftStatement,
        new tree::MoveStm(framePointerCopy, _frame->GetFrameAddress()));
  }
  // escape and non-escape formals
  for (int i = 0; i < formals.size(); ++i) {
    if (formals.at(i)) { // escape
      _frame->formalAccesses_.push_back(new InFrameAccess(frameOffset));
      destinationExppression = new tree::MemExp(new tree::BinopExp(
          tree::MINUS_OP, framePointerExpression,
          new tree::ConstExp((i + 1) * _frame->GetWordSize())));
      // claculate offset from frame pointer (rbp)
      frameOffset += _frame->GetWordSize();
      _frame->localVariableCount_++;
    } else {
      // non-escape
      temp::Temp *reg = temp::TempFactory::NewTemp();
      _frame->formalAccesses_.push_back(new InRegAccess(reg));
      destinationExppression = new tree::TempExp(reg);
    }
    if (i < ArgRegCount) {
      singleViewShift = new tree::MoveStm(
          destinationExppression,
          new tree::TempExp(reg_manager->ArgRegs()->NthTemp(i)));
    } else {
      // *fp is return address
      singleViewShift =
          new tree::MoveStm(destinationExppression,
                            new tree::MemExp(new tree::BinopExp(
                                tree::PLUS_OP, framePointerCopy,
                                new tree::ConstExp((i - ArgRegCount + 1) *
                                                   _frame->GetWordSize()))));
    }
    _frame->viewShiftStatement =
        new tree::SeqStm(_frame->viewShiftStatement, singleViewShift);
  }

  temp::TempList *calleeSaves = reg_manager->CalleeSaves();

  temp::Temp *storedReg;
  tree::Stm *saveStm, *restoreStm;

  auto calleeSavesEnd = calleeSaves->GetList().end();
  storedReg = temp::TempFactory::NewTemp();
  // save and restore return address
  _frame->restoreCalleeSavesStatement =
      new tree::MoveStm(new tree::TempExp(calleeSaves->GetList().front()),
                        new tree::TempExp(storedReg));
  _frame->saveCalleeSavesStatement =
      new tree::MoveStm(new tree::TempExp(storedReg),
                        new tree::TempExp(calleeSaves->GetList().front()));

  for (auto regIt = std::next(calleeSaves->GetList().begin());
       regIt != calleeSavesEnd; ++regIt) {
    storedReg = temp::TempFactory::NewTemp();
    saveStm = new tree::MoveStm(new tree::TempExp(storedReg),
                                new tree::TempExp(*regIt));
    restoreStm = new tree::MoveStm(new tree::TempExp(*regIt),
                                   new tree::TempExp(storedReg));
    _frame->restoreCalleeSavesStatement =
        new tree::SeqStm(restoreStm, _frame->restoreCalleeSavesStatement);
    _frame->saveCalleeSavesStatement =
        new tree::SeqStm(saveStm, _frame->saveCalleeSavesStatement);
  }

  return _frame;
}
// Function to get the current access expression from an Access object in the
// context of a frame
tree::Exp *GetCurrentAccessExpression(Access *acc, Frame *frame) {
  if (typeid(*acc) == typeid(InFrameAccess)) {
    // Cast access to InFrameAccess and calculate memory expression
    InFrameAccess *frameAcc = static_cast<InFrameAccess *>(acc);
    return new tree::MemExp(
        new tree::BinopExp(tree::MINUS_OP, frame->GetFrameAddress(),
                           new tree::ConstExp(frameAcc->offset)));
  } else {
    // Cast access to InRegAccess and return the register expression
    InRegAccess *regAcc = static_cast<InRegAccess *>(acc);
    return new tree::TempExp(regAcc->reg);
  }
}

// Function to get the access expression from an Access object using a frame
// pointer
tree::Exp *GetAccessExpression(Access *acc, tree::Exp *fp) {
  if (typeid(*acc) != typeid(InFrameAccess)) {
    // Cast access to InRegAccess and return the register expression
    InRegAccess *regAcc = static_cast<InRegAccess *>(acc);
    return new tree::TempExp(regAcc->reg);
  } else {
    // Cast access to InFrameAccess and calculate memory expression with frame
    // pointer
    InFrameAccess *frameAcc = static_cast<InFrameAccess *>(acc);
    return new tree::MemExp(new tree::BinopExp(
        tree::MINUS_OP, fp, new tree::ConstExp(frameAcc->offset)));
  }
}

// Function to create an external call expression
tree::Exp *CreateExternalFunctionCall(std::string functionName,
                                      tree::ExpList *arguments) {
  // Create a CallExp with the function name and arguments
  return new tree::CallExp(
      new tree::NameExp(temp::LabelFactory::NamedLabel(functionName)),
      arguments);
}

// Function to create a statement for procedure entry and exit
tree::Stm *GenerateProcedureEntryExitSequence(Frame *currentFrame,
                                              tree::Stm *procedureBody) {
  // Sequence statements for callee saves, view shift, and restore
  procedureBody =
      new tree::SeqStm(currentFrame->saveCalleeSavesStatement, procedureBody);
  procedureBody =
      new tree::SeqStm(currentFrame->viewShiftStatement, procedureBody);
  procedureBody = new tree::SeqStm(procedureBody,
                                   currentFrame->restoreCalleeSavesStatement);
  return procedureBody;
}

// Function to append a return sink instruction to a list of assembly
// instructions
assem::InstrList *PrepareProcedureInstructions(assem::InstrList *body) {
  // Create a return sink operation instruction
  assem::Instr *returnSink =
      new assem::OperInstr("", nullptr, reg_manager->ReturnSink(), nullptr);
  body->Append(returnSink);
  return body;
}

// Function to create a procedure object with prologue and epilogue
assem::Proc *
BuildCompleteProcedure(Frame *procedureFrame,
                       assem::InstrList *procedureBodyInstructions) {
  std::stringstream prologue, epilogue;

  // Calculate frame size
  int frameSize = (procedureFrame->localVariableCount_ +
                   procedureFrame->maxOutgoingArguments_) *
                  procedureFrame->GetWordSize();
  prologue << ".set " << procedureFrame->frameSizeLabel_->Name() << ", "
           << frameSize << "\n";

  // Add function label and adjust stack pointer
  prologue << procedureFrame->GetFrameLabel() << ":\n";
  if (frameSize != 0)
    prologue << "subq $" << frameSize << ", "
             << *reg_manager->temp_map_->Look(reg_manager->StackPointer())
             << "\n";

  // Reset stack pointer and return instruction
  if (frameSize != 0)
    epilogue << "addq $" << frameSize << ", "
             << *reg_manager->temp_map_->Look(reg_manager->StackPointer())
             << "\n";
  epilogue << "retq\n";

  return new assem::Proc(prologue.str(), procedureBodyInstructions,
                         epilogue.str());
}

} // namespace frame
