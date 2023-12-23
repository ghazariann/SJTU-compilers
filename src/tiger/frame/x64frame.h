#include <unordered_map>

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

const int WORD_SIZE_X64_ = 8;

namespace frame {
class X64RegManager : public RegManager {
public:
  X64RegManager();

  temp::TempList *Registers() override;
  temp::TempList *ArgRegs() override;
  temp::TempList *CallerSaves() override;
  temp::TempList *CalleeSaves() override;
  temp::TempList *ReturnSink() override;
  temp::Temp *FramePointer() override;
  temp::Temp *StackPointer() override;
  temp::Temp *ReturnValue() override;
  temp::Temp *GetArithmeticRegister() override;
  temp::Temp *ProgramCounter() override;
  int WordSize() override;

  enum Register {
    RAX,
    RBX,
    RCX,
    RDX,
    RSI,
    RDI,
    RBP,
    RSP,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,
    RIP,
    REG_COUNT,
  };

  static std::unordered_map<Register, std::string> registerMap;

private:
  static const int WORD_SIZE = WORD_SIZE_X64_;
};

Frame *NewFrame(temp::Label *name, std::vector<bool> formals);
tree::Exp *GetCurrentAccessExpression(Access *acc, Frame *frame);
tree::Exp *GetAccessExpression(Access *acc, tree::Exp *fp);
// Function to create an expression for an external function call
tree::Exp *CreateExternalFunctionCall(std::string functionName,
                                      tree::ExpList *arguments);

// Function to generate the entry and exit sequence for a procedure's body
// statement
tree::Stm *GenerateProcedureEntryExitSequence(Frame *currentFrame,
                                              tree::Stm *procedureBody);

// Function to prepare the instruction list for a procedure with necessary
// pre-return processing
assem::InstrList *
PrepareProcedureInstructions(assem::InstrList *procedureInstructions);

// Function to construct the complete procedure with prologue and epilogue
assem::Proc *
BuildCompleteProcedure(Frame *procedureFrame,
                       assem::InstrList *procedureBodyInstructions);

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
