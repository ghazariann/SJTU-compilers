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
  int WordSize() override;
  int RegisterCount() override;

  enum Register {
    RSP,
    RBP,
    RAX,
    RBX,
    RCX,
    RDX,
    RSI,
    RDI,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,
    REG_COUNT,
  };

  static std::unordered_map<Register, std::string> registerMap;

private:
  static const int WORD_SIZE = WORD_SIZE_X64_;
};

Frame *NewFrame(temp::Label *name, std::vector<bool> formals);
tree::Stm *ProcEntryExit1(Frame *frame, tree::Stm *stm);
} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
