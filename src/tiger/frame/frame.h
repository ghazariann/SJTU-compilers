#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/translate/tree.h"

namespace frame {

class RegManager {
public:
  RegManager() : temp_map_(temp::Map::Empty()) {}

  temp::Temp *GetRegister(int regno) { return regs_[regno]; }

  /**
   * Get general-purpose registers except RSI
   * NOTE: returned temp list should be in the order of calling convention
   * @return general-purpose registers
   */
  [[nodiscard]] virtual temp::TempList *Registers() = 0;

  /**
   * Get registers which can be used to hold arguments
   * NOTE: returned temp list must be in the order of calling convention
   * @return argument registers
   */
  [[nodiscard]] virtual temp::TempList *ArgRegs() = 0;

  /**
   * Get caller-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return caller-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CallerSaves() = 0;

  /**
   * Get callee-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return callee-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CalleeSaves() = 0;

  /**
   * Get return-sink registers
   * @return return-sink registers
   */
  [[nodiscard]] virtual temp::TempList *ReturnSink() = 0;

  /**
   * Get word size
   */
  [[nodiscard]] virtual int WordSize() = 0;
  [[nodiscard]] virtual int RegisterCount() = 0;

  [[nodiscard]] virtual temp::Temp *FramePointer() = 0;

  [[nodiscard]] virtual temp::Temp *StackPointer() = 0;

  [[nodiscard]] virtual temp::Temp *ReturnValue() = 0;

  [[nodiscard]] virtual temp::Temp *GetArithmeticRegister() = 0;

  temp::Map *temp_map_;

protected:
  std::vector<temp::Temp *> regs_;
};

class Frame;
class Access {
public:
  /* TODO: Put your lab5 code here */
  virtual std::string ConsumeAccess(Frame *frame) = 0;
  virtual ~Access() = default;
};

class Frame {
protected:
  Frame() = default;
  explicit Frame(temp::Label *frameLabel)
      : frameLabel_(frameLabel), localVariableCount_(0),
        maxOutgoingArguments_(0) {}

  virtual ~Frame() = default;
  // Word size is machine dependent and initialized in subclass
  int wordSize_;

public:
  // Get the word size for the frame
  virtual int GetWordSize() const = 0;
  // Get the label associated with the frame
  virtual std::string GetFrameLabel() const = 0;
  // Allocate a new local variable in the frame
  virtual Access *AllocateLocal(bool escapes) = 0;
  // Get the address expression of the frame
  virtual tree::Exp *GetFrameAddress() const = 0;

  // Get the stack offset expression for a given frame offset
  virtual tree::Exp *GetStackOffset(int frameOffset) const = 0;
  // Label for the frame
  temp::Label *frameLabel_;
  std::vector<Access *> formalAccesses_;
  std::vector<Access *> localAccesses_;
  int localVariableCount_;
  // Label for the frame size, altered when frame size is known
  temp::Label *frameSizeLabel_;
  // Statement for view shift operations
  tree::Stm *viewShiftStatement;
  // Statement for saving callee-saved registers
  tree::Stm *saveCalleeSavesStatement;
  // Statement for restoring callee-saved registers
  tree::Stm *restoreCalleeSavesStatement;
  // Maximum number of outgoing arguments in any call within the frame
  int maxOutgoingArguments_;
};

/**
 * Fragments
 */

class Frag {
public:
  virtual ~Frag() = default;

  enum OutputPhase {
    Proc,
    String,
  };

  /**
   *Generate assembly for main program
   * @param out FILE object for output assembly file
   */
  virtual void OutputAssem(FILE *out, OutputPhase phase,
                           bool need_ra) const = 0;
};

class StringFrag : public Frag {
public:
  temp::Label *label_;
  std::string str_;

  StringFrag(temp::Label *label, std::string str)
      : label_(label), str_(std::move(str)) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class ProcFrag : public Frag {
public:
  tree::Stm *body_;
  Frame *frame_;

  ProcFrag(tree::Stm *body, Frame *frame) : body_(body), frame_(frame) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class Frags {
public:
  Frags() = default;
  void PushBack(Frag *frag) { frags_.emplace_back(frag); }
  const std::list<Frag *> &GetList() { return frags_; }

private:
  std::list<Frag *> frags_;
};

} // namespace frame

#endif