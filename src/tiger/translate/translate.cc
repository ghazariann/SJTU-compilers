#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/x64frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  /* TODO: Put your lab5 code here */
  frame::Frame *frm = level->frame_;
  frame::Access *acs = frm->AllocateLocal(escape);
  return new Access(level, acs);
}

class Cx {
public:
  PatchList trues_;
  PatchList falses_;
  tree::Stm *stm_;

  Cx(PatchList trues, PatchList falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    return exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    tree::CjumpStm *cjump_stm =
        new tree::CjumpStm(tree::NE_OP, new tree::ConstExp(0), exp_, t, f);
    // Create patch lists for the true and false labels
    PatchList trues({&cjump_stm->true_label_});
    PatchList falses({&cjump_stm->false_label_});

    return Cx(trues, falses, cjump_stm);
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    errormsg->Error(errormsg->GetTokPos(),
                    "No result for conditional statement");
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(PatchList trues, PatchList falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    temp::Temp *reg = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();

    // Patch the true and false lists with the new labels
    cx_.trues_.DoPatch(t);
    cx_.falses_.DoPatch(f);

    return new tree::EseqExp(
        new tree::MoveStm(new tree::TempExp(reg), new tree::ConstExp(1)),
        new tree::EseqExp(
            cx_.stm_,
            new tree::EseqExp(
                new tree::LabelStm(f),
                new tree::EseqExp(new tree::MoveStm(new tree::TempExp(reg),
                                                    new tree::ConstExp(0)),
                                  new tree::EseqExp(new tree::LabelStm(t),
                                                    new tree::TempExp(reg))))));
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return cx_.stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    return cx_;
  }
};
void ProcEntryExit(Level *level, Exp *body) {
  frame::ProcFrag *fragments = new frame::ProcFrag(body->UnNx(), level->frame_);
  frags->PushBack(fragments);
}

void ProgTr::Translate() {
  /* TODO: Put your lab5 code here */
  temp::Label *mainLabel = temp::LabelFactory::NamedLabel("tigermain");
  frame::Frame *newFrame = frame::NewFrame(mainLabel, std::vector<bool>());
  Level *mainLevel = new Level(newFrame, main_level_.get());
  tr::ExpAndTy *treeExpAndTy = absyn_tree_->Translate(
      venv_.get(), tenv_.get(), mainLevel, nullptr, errormsg_.get());
  ProcEntryExit(mainLevel, treeExpAndTy->exp_);
}

} // namespace tr

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *currentLevel, temp::Label *label,
                                   err::ErrorMsg *errorMessage) const {
  env::EnvEntry *environmentEntry = venv->Look(sym_);
  if (!environmentEntry) {
    errorMessage->Error(pos_, "variable %s does not exist",
                        sym_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  if (typeid(*environmentEntry) != typeid(env::VarEntry)) {
    errorMessage->Error(pos_, "%s is not a variable", sym_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  env::VarEntry *variableEntry = static_cast<env::VarEntry *>(environmentEntry);
  tr::Access *declarationAccess = variableEntry->access_;
  tr::Level *declarationLevel = declarationAccess->level_;

  if (currentLevel == declarationLevel) {
    tree::Exp *variableExpression = frame::GetCurrentAccessExpression(
        declarationAccess->access_, declarationLevel->frame_);
    return new tr::ExpAndTy(new tr::ExExp(variableExpression),
                            variableEntry->ty_);
  }

  tree::Exp *staticLink = frame::GetCurrentAccessExpression(
      currentLevel->frame_->formalAccesses_.front(), currentLevel->frame_);
  currentLevel = currentLevel->parent_;

  while (currentLevel != declarationLevel) {
    staticLink = frame::GetAccessExpression(
        currentLevel->frame_->formalAccesses_.front(), staticLink);
    currentLevel = currentLevel->parent_;
  }

  tree::Exp *variableExpression =
      frame::GetAccessExpression(declarationAccess->access_, staticLink);
  return new tr::ExpAndTy(new tr::ExExp(variableExpression),
                          variableEntry->ty_);
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr varEnvironment,
                                  env::TEnvPtr typeEnvironment,
                                  tr::Level *currentLevel, temp::Label *label,
                                  err::ErrorMsg *errorMessage) const {
  tr::ExpAndTy *variableExpAndType = var_->Translate(
      varEnvironment, typeEnvironment, currentLevel, label, errorMessage);
  tree::Exp *variableExp = variableExpAndType->exp_->UnEx();
  type::Ty *variableType = variableExpAndType->ty_;

  if (typeid(*variableType->ActualTy()) != typeid(type::RecordTy)) {
    errorMessage->Error(var_->pos_, "not a record type"); // test25
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  type::RecordTy *recordType =
      static_cast<type::RecordTy *>(variableType->ActualTy());
  int fieldIndex = 0;
  for (type::Field *field : recordType->fields_->GetList()) {
    if (field->name_->Name() == sym_->Name()) {
      tree::Exp *fieldExp = new tree::MemExp(new tree::BinopExp(
          tree::PLUS_OP, variableExp,
          new tree::ConstExp(fieldIndex *
                             currentLevel->frame_->GetWordSize())));
      return new tr::ExpAndTy(new tr::ExExp(fieldExp), field->ty_->ActualTy());
    }
    fieldIndex++;
  }

  errorMessage->Error(pos_, "there is no field named %s", sym_->Name().data());
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::VoidTy::Instance());
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr varEnvironment,
                                      env::TEnvPtr typeEnvironment,
                                      tr::Level *currentLevel,
                                      temp::Label *label,
                                      err::ErrorMsg *errorMessage) const {
  tr::ExpAndTy *variableExpAndType = var_->Translate(
      varEnvironment, typeEnvironment, currentLevel, label, errorMessage);
  tree::Exp *variableExp = variableExpAndType->exp_->UnEx();
  type::Ty *variableType = variableExpAndType->ty_;

  if (typeid(*variableType->ActualTy()) != typeid(type::ArrayTy)) {
    errorMessage->Error(var_->pos_, "not an array type");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  tr::ExpAndTy *subscriptExpAndType = subscript_->Translate(
      varEnvironment, typeEnvironment, currentLevel, label, errorMessage);
  tree::Exp *subscriptExp = subscriptExpAndType->exp_->UnEx();

  if (typeid(*subscriptExpAndType->ty_->ActualTy()) != typeid(type::IntTy)) {
    errorMessage->Error(subscript_->pos_, "subscript is not an integer");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  type::ArrayTy *arrayType =
      static_cast<type::ArrayTy *>(variableType->ActualTy());
  tree::Exp *arrayExp = new tree::MemExp(new tree::BinopExp(
      tree::PLUS_OP, variableExp,
      new tree::BinopExp(
          tree::MUL_OP, subscriptExp,
          new tree::ConstExp(currentLevel->frame_->GetWordSize()))));
  return new tr::ExpAndTy(new tr::ExExp(arrayExp), arrayType->ty_);
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(val_)),
                          type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  temp::Label *stringLabel = temp::LabelFactory::NewLabel();
  frags->PushBack(new frame::StringFrag(stringLabel, str_));
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(stringLabel)),
                          type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // Look up the function in the environment
  env::EnvEntry *entry = venv->Look(func_);
  if (!entry || typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s",
                    func_->Name().data()); // test18
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  // Cast the found entry to FunEntry
  env::FunEntry *funcEntry = static_cast<env::FunEntry *>(entry);

  // Prepare the arguments list for the function call
  tree::ExpList *args = new tree::ExpList();
  tree::Exp *funcExp;

  // Check if the function is a user-defined function with a label
  if (funcEntry->label_) {
    // Handling the static link
    if (funcEntry->level_->parent_ == level) {
      // The calling function is the parent of the called function; use the
      // current frame pointer
      args->Append(level->frame_->GetFrameAddress());
    } else {
      // The calling function is not the parent; follow static links to find the
      // correct frame
      tree::Exp *staticLink = frame::GetCurrentAccessExpression(
          level->frame_->formalAccesses_.front(), level->frame_);
      tr::Level *currentLevel = level->parent_; // Start from the parent level

      // Traverse the static link chain until reaching the level of the function
      while (currentLevel && currentLevel != funcEntry->level_->parent_) {
        staticLink = frame::GetAccessExpression(
            currentLevel->parent_->frame_->formalAccesses_.front(), staticLink);
        currentLevel = currentLevel->parent_;
      }

      // Check if the correct level is found
      if (currentLevel) {
        args->Append(staticLink);
      } else {
        // The level of the function is not found in the static link chain
        errormsg->Error(pos_, "%s cannot call %s",
                        level->frame_->GetFrameLabel().data(),
                        funcEntry->level_->frame_->GetFrameLabel().data());
        return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                                type::VoidTy::Instance());
      }
    }
    funcExp = new tree::NameExp(funcEntry->label_);
  } else {
    // For external function calls
    funcExp = new tree::NameExp(func_);
  }

  // Translate and append each argument to the arguments list
  for (Exp *arg : args_->GetList()) {
    tree::Exp *argExp =
        arg->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx();
    args->Append(argExp);
  }

  // Adjust the maximum number of outgoing arguments in the frame
  level->frame_->maxOutgoingArguments_ =
      std::max(level->frame_->maxOutgoingArguments_,
               static_cast<int>(args->GetList().size()) -
                   static_cast<int>(reg_manager->ArgRegs()->GetList().size()));

  // Create the call expression
  tree::Exp *callExp = new tree::CallExp(funcExp, args);
  return new tr::ExpAndTy(new tr::ExExp(callExp), funcEntry->result_);
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *leftExpressionType =
      left_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *rightExpressionType =
      right_->Translate(venv, tenv, level, label, errormsg);
  tree::Exp *leftExpression = leftExpressionType->exp_->UnEx();
  tree::Exp *rightExpression = rightExpressionType->exp_->UnEx();

  if (oper_ == absyn::AND_OP) {
    tr::Cx conditionalExp = leftExpressionType->exp_->UnCx(errormsg);
    tr::ExpAndTy *falseExpressionType = new tr::ExpAndTy(
        new tr::ExExp(new tree::ConstExp(0)), type::IntTy::Instance());
    temp::Label *convergenceLabel = temp::LabelFactory::NewLabel();
    std::vector<temp::Label *> *convergenceJumps =
        new std::vector<temp::Label *>{convergenceLabel};
    temp::Temp *resultRegister = temp::TempFactory::NewTemp();
    tree::Exp *resultRegisterExp = new tree::TempExp(resultRegister);
    tree::Exp *conditionalExpression = new tree::EseqExp(
        conditionalExp.stm_,
        new tree::EseqExp(
            new tree::LabelStm(*(conditionalExp.trues_.GetFront())),
            new tree::EseqExp(
                new tree::MoveStm(resultRegisterExp, rightExpression),
                new tree::EseqExp(
                    new tree::JumpStm(new tree::NameExp(convergenceLabel),
                                      convergenceJumps),
                    new tree::EseqExp(
                        new tree::LabelStm(
                            *(conditionalExp.falses_.GetFront())),
                        new tree::EseqExp(
                            new tree::MoveStm(
                                resultRegisterExp,
                                falseExpressionType->exp_->UnEx()),
                            new tree::EseqExp(
                                new tree::LabelStm(convergenceLabel),
                                resultRegisterExp)))))));
    return new tr::ExpAndTy(new tr::ExExp(conditionalExpression),
                            rightExpressionType->ty_);
  }

  if (oper_ == absyn::OR_OP) {
    tr::Cx conditionalExp = leftExpressionType->exp_->UnCx(errormsg);
    tr::ExpAndTy *falseExpressionType = new tr::ExpAndTy(
        new tr::ExExp(new tree::ConstExp(1)), type::IntTy::Instance());
    temp::Label *convergenceLabel = temp::LabelFactory::NewLabel();
    std::vector<temp::Label *> *convergenceJumps =
        new std::vector<temp::Label *>{convergenceLabel};
    temp::Temp *resultRegister = temp::TempFactory::NewTemp();
    tree::Exp *resultRegisterExp = new tree::TempExp(resultRegister);
    tree::Exp *conditionalExpression = new tree::EseqExp(
        conditionalExp.stm_,
        new tree::EseqExp(
            new tree::LabelStm(*(conditionalExp.trues_.GetFront())),
            new tree::EseqExp(
                new tree::MoveStm(
                    resultRegisterExp,
                    falseExpressionType->exp_->UnEx()), // represents 1
                new tree::EseqExp(
                    new tree::JumpStm(new tree::NameExp(convergenceLabel),
                                      convergenceJumps),
                    new tree::EseqExp(
                        new tree::LabelStm(
                            *(conditionalExp.falses_.GetFront())),
                        new tree::EseqExp(
                            new tree::MoveStm(
                                resultRegisterExp,
                                rightExpression), //  falseExpressionType->exp_->UnEx()
                            new tree::EseqExp(
                                new tree::LabelStm(convergenceLabel),
                                resultRegisterExp)))))));
    return new tr::ExpAndTy(new tr::ExExp(conditionalExpression),
                            falseExpressionType->ty_);
  }
  tree::CjumpStm *conditionalJumpStatement;
  tr::Exp *finalExpression;

  // Check for type compatibility between left and right expressions
  if (leftExpressionType->ty_->IsSameType(rightExpressionType->ty_)) {
    // Handling string operations
    if (typeid(*leftExpressionType->ty_->ActualTy()) ==
        typeid(type::StringTy)) {
      tree::ExpList *argumentList =
          new tree::ExpList({leftExpression, rightExpression});
      std::string stringComparisonFunction = "string_equal";

      switch (oper_) {
      case absyn::EQ_OP:
        finalExpression = new tr::ExExp(frame::CreateExternalFunctionCall(
            stringComparisonFunction, argumentList));
        break;
      case absyn::NEQ_OP:
        finalExpression = new tr::ExExp(
            new tree::BinopExp(tree::MINUS_OP, new tree::ConstExp(1),
                               frame::CreateExternalFunctionCall(
                                   stringComparisonFunction, argumentList)));
        break;
      default:
        errormsg->Error(pos_, "unexpected binary token %d", oper_);
        return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                                type::VoidTy::Instance());
      }
    } else {
      // Handling numeric operations
      switch (oper_) {
      case absyn::PLUS_OP:
        finalExpression = new tr::ExExp(
            new tree::BinopExp(tree::PLUS_OP, leftExpression, rightExpression));
        break;
      case absyn::MINUS_OP:
        finalExpression = new tr::ExExp(new tree::BinopExp(
            tree::MINUS_OP, leftExpression, rightExpression));
        break;
      case absyn::TIMES_OP:
        finalExpression = new tr::ExExp(
            new tree::BinopExp(tree::MUL_OP, leftExpression, rightExpression));
        break;
      case absyn::DIVIDE_OP:
        finalExpression = new tr::ExExp(
            new tree::BinopExp(tree::DIV_OP, leftExpression, rightExpression));
        break;
      case absyn::EQ_OP:
        conditionalJumpStatement = new tree::CjumpStm(
            tree::EQ_OP, leftExpression, rightExpression, nullptr, nullptr);
        finalExpression = new tr::CxExp(
            tr::PatchList({&conditionalJumpStatement->true_label_}),
            tr::PatchList({&conditionalJumpStatement->false_label_}),
            conditionalJumpStatement);
        break;
      case absyn::NEQ_OP:
        conditionalJumpStatement = new tree::CjumpStm(
            tree::NE_OP, leftExpression, rightExpression, nullptr, nullptr);
        finalExpression = new tr::CxExp(
            tr::PatchList({&conditionalJumpStatement->true_label_}),
            tr::PatchList({&conditionalJumpStatement->false_label_}),
            conditionalJumpStatement);
        break;
      case absyn::GE_OP:
        conditionalJumpStatement = new tree::CjumpStm(
            tree::GE_OP, leftExpression, rightExpression, nullptr, nullptr);
        finalExpression = new tr::CxExp(
            tr::PatchList({&conditionalJumpStatement->true_label_}),
            tr::PatchList({&conditionalJumpStatement->false_label_}),
            conditionalJumpStatement);
        break;
      case absyn::GT_OP:
        conditionalJumpStatement = new tree::CjumpStm(
            tree::GT_OP, leftExpression, rightExpression, nullptr, nullptr);
        finalExpression = new tr::CxExp(
            tr::PatchList({&conditionalJumpStatement->true_label_}),
            tr::PatchList({&conditionalJumpStatement->false_label_}),
            conditionalJumpStatement);
        break;

      case absyn::LE_OP:
        conditionalJumpStatement = new tree::CjumpStm(
            tree::LE_OP, leftExpression, rightExpression, nullptr, nullptr);
        finalExpression = new tr::CxExp(
            tr::PatchList({&conditionalJumpStatement->true_label_}),
            tr::PatchList({&conditionalJumpStatement->false_label_}),
            conditionalJumpStatement);
        break;
      case absyn::LT_OP:
        conditionalJumpStatement = new tree::CjumpStm(
            tree::LT_OP, leftExpression, rightExpression, nullptr, nullptr);
        finalExpression = new tr::CxExp(
            tr::PatchList({&conditionalJumpStatement->true_label_}),
            tr::PatchList({&conditionalJumpStatement->false_label_}),
            conditionalJumpStatement);
        break;
      default:
        errormsg->Error(pos_, "unexpected binary token %d", oper_);
        return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                                type::VoidTy::Instance());
      }
    }
  }

  if (!finalExpression) {
    errormsg->Error(pos_, "binary operation type mismatch");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  return new tr::ExpAndTy(finalExpression, type::IntTy::Instance());
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  type::Ty *typeInfo = tenv->Look(typ_);
  if (!typeInfo) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  } else if (typeid(*typeInfo->ActualTy()) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "type %s is not a record", typ_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  type::RecordTy *recordType =
      static_cast<type::RecordTy *>(typeInfo->ActualTy());
  auto recordFields = recordType->fields_->GetList();
  auto exprFields = fields_->GetList();
  int fieldCount = recordFields.size();
  tree::Exp *recordExp = new tree::TempExp(temp::TempFactory::NewTemp());
  tree::ExpList *allocArgs = new tree::ExpList(
      {new tree::ConstExp(fieldCount * level->frame_->GetWordSize())});
  tree::Stm *allocStm = new tree::MoveStm(
      recordExp, frame::CreateExternalFunctionCall("alloc_record", allocArgs));

  if (recordFields.empty() && exprFields.empty()) {
    return new tr::ExpAndTy(
        new tr::ExExp(new tree::EseqExp(allocStm, recordExp)), recordType);
  } else if (recordFields.empty() || exprFields.empty()) {
    errormsg->Error(pos_, "field type mismatch");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  type::Field *lastField = recordFields.back();
  EField *lastEField = exprFields.back();
  tr::ExpAndTy *lastEFieldExpTy =
      lastEField->exp_->Translate(venv, tenv, level, label, errormsg);
  if (!(lastField->ty_->IsSameType(lastEFieldExpTy->ty_))) {
    errormsg->Error(pos_, "field type mismatch");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  tree::Stm *stm = new tree::MoveStm(
      new tree::MemExp(new tree::BinopExp(
          tree::PLUS_OP, recordExp,
          new tree::ConstExp((--fieldCount) * level->frame_->GetWordSize()))),
      lastEFieldExpTy->exp_->UnEx());
  auto fieldIt = ++recordFields.rbegin();
  auto eFieldIt = ++exprFields.rbegin();
  for (; fieldIt != recordFields.rend() && eFieldIt != exprFields.rend();
       ++fieldIt, ++eFieldIt) {
    tr::ExpAndTy *eFieldExpTy =
        (*eFieldIt)->exp_->Translate(venv, tenv, level, label, errormsg);
    if (!(eFieldExpTy->ty_->IsSameType((*fieldIt)->ty_))) {
      errormsg->Error(pos_, "field type mismatch");
      return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                              type::VoidTy::Instance());
    }
    tree::Exp *eFieldExp = eFieldExpTy->exp_->UnEx();
    stm = new tree::SeqStm(
        new tree::MoveStm(
            new tree::MemExp(new tree::BinopExp(
                tree::PLUS_OP, recordExp,
                new tree::ConstExp((--fieldCount) *
                                   level->frame_->GetWordSize()))),
            eFieldExpTy->exp_->UnEx()),
        stm);
  }

  if (fieldIt != recordFields.rend() || eFieldIt != exprFields.rend()) {
    errormsg->Error(pos_, "fields mismatch");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  stm = new tree::SeqStm(allocStm, stm);
  tree::Exp *resultExp = new tree::EseqExp(stm, recordExp);

  return new tr::ExpAndTy(new tr::ExExp(resultExp), recordType);
}
tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  tree::ExpList *seqExps = new tree::ExpList();
  tr::ExpAndTy *expAndTy;
  for (auto exp : seq_->GetList()) {
    expAndTy = exp->Translate(venv, tenv, level, label, errormsg);
    seqExps->Append(expAndTy->exp_->UnEx());
  }

  tree::Exp *resultExp = seqExps->GetList().back();
  for (auto it = ++seqExps->GetList().rbegin(); it != seqExps->GetList().rend();
       ++it) {
    resultExp = new tree::EseqExp(new tree::ExpStm(*it), resultExp);
  }

  return new tr::ExpAndTy(new tr::ExExp(resultExp), expAndTy->ty_);
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *varExpAndTy =
      var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *expExpAndTy =
      exp_->Translate(venv, tenv, level, label, errormsg);
  if (!(varExpAndTy->ty_->IsSameType(expExpAndTy->ty_))) {
    errormsg->Error(exp_->pos_, "unmatched assign exp");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  tree::Exp *varExp = varExpAndTy->exp_->UnEx();
  tree::Exp *expExp = expExpAndTy->exp_->UnEx();
  tree::Stm *assignStm = new tree::MoveStm(varExp, expExp);
  return new tr::ExpAndTy(new tr::NxExp(assignStm), type::VoidTy::Instance());
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *testExpTy =
      test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *thenExpTy =
      then_->Translate(venv, tenv, level, label, errormsg);
  tr::Cx testCx = testExpTy->exp_->UnCx(errormsg);

  temp::Label *trueLabel = temp::LabelFactory::NewLabel();
  testCx.trues_.DoPatch(trueLabel);

  temp::Label *falseLabel = temp::LabelFactory::NewLabel();
  testCx.falses_.DoPatch(falseLabel);

  if (!elsee_) {
    if (typeid(*thenExpTy->ty_) != typeid(type::VoidTy)) {
      errormsg->Error(then_->pos_, "if with no else must return no value");
      return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                              type::VoidTy::Instance());
    }

    tree::Stm *stm = new tree::SeqStm(
        testCx.stm_,
        new tree::SeqStm(new tree::LabelStm(trueLabel),
                         new tree::SeqStm(thenExpTy->exp_->UnNx(),
                                          new tree::LabelStm(falseLabel))));
    return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());
  } else {
    tr::ExpAndTy *elseExpTy =
        elsee_->Translate(venv, tenv, level, label, errormsg);

    if (!(thenExpTy->ty_->IsSameType(elseExpTy->ty_))) {
      errormsg->Error(elsee_->pos_, "then exp and else exp type mismatch");
      return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                              type::VoidTy::Instance());
    }

    temp::Label *convergeLabel = temp::LabelFactory::NewLabel();
    std::vector<temp::Label *> *convergeJumps =
        new std::vector<temp::Label *>{convergeLabel};

    if (typeid(*thenExpTy->ty_) == typeid(type::VoidTy)) {
      tree::Stm *stm = new tree::SeqStm(
          testCx.stm_,
          new tree::SeqStm(
              new tree::LabelStm(trueLabel),
              new tree::SeqStm(
                  thenExpTy->exp_->UnNx(),
                  new tree::SeqStm(
                      new tree::JumpStm(new tree::NameExp(convergeLabel),
                                        convergeJumps),
                      new tree::SeqStm(
                          new tree::LabelStm(falseLabel),
                          new tree::SeqStm(
                              elseExpTy->exp_->UnNx(),
                              new tree::LabelStm(convergeLabel)))))));
      return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());

    } else {
      temp::Temp *reg = temp::TempFactory::NewTemp();
      tree::Exp *regExp = new tree::TempExp(reg);
      tree::Exp *exp = new tree::EseqExp(
          testCx.stm_,
          new tree::EseqExp(
              new tree::LabelStm(trueLabel),
              new tree::EseqExp(
                  new tree::MoveStm(regExp, thenExpTy->exp_->UnEx()),
                  new tree::EseqExp(
                      new tree::JumpStm(new tree::NameExp(convergeLabel),
                                        convergeJumps),
                      new tree::EseqExp(
                          new tree::LabelStm(falseLabel),
                          new tree::EseqExp(
                              new tree::MoveStm(regExp,
                                                elseExpTy->exp_->UnEx()),
                              new tree::EseqExp(
                                  new tree::LabelStm(convergeLabel),
                                  regExp)))))));
      return new tr::ExpAndTy(new tr::ExExp(exp), thenExpTy->ty_);
    }
  }
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *testExpTy =
      test_->Translate(venv, tenv, level, label, errormsg);
  tr::Cx testCx = testExpTy->exp_->UnCx(errormsg);

  temp::Label *testLabel = temp::LabelFactory::NewLabel();
  temp::Label *bodyLabel = temp::LabelFactory::NewLabel();
  temp::Label *doneLabel = temp::LabelFactory::NewLabel();
  tr::ExpAndTy *bodyExpTy =
      body_->Translate(venv, tenv, level, doneLabel, errormsg);

  if (typeid(*bodyExpTy->ty_) != typeid(type::VoidTy)) {
    errormsg->Error(body_->pos_, "while body must produce no value");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  testCx.trues_.DoPatch(bodyLabel);
  testCx.falses_.DoPatch(doneLabel);

  std::vector<temp::Label *> *testJumps =
      new std::vector<temp::Label *>{testLabel};
  tree::Stm *testJumpStm =
      new tree::JumpStm(new tree::NameExp(testLabel), testJumps);

  tree::Stm *whileStm = new tree::SeqStm(
      new tree::LabelStm(testLabel),
      new tree::SeqStm(
          testCx.stm_,
          new tree::SeqStm(
              new tree::LabelStm(bodyLabel),
              new tree::SeqStm(
                  bodyExpTy->exp_->UnNx(),
                  new tree::SeqStm(testJumpStm,
                                   new tree::LabelStm(doneLabel))))));
  return new tr::ExpAndTy(new tr::NxExp(whileStm), type::VoidTy::Instance());
}
tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  sym::Symbol *limitSymbol = sym::Symbol::UniqueSymbol("limit");

  DecList *declarations = new DecList();
  VarDec *hiDec = new VarDec(hi_->pos_, limitSymbol, nullptr, hi_);
  hiDec->escape_ = false;
  declarations->Prepend(hiDec);
  VarDec *loDec = new VarDec(lo_->pos_, var_, nullptr, lo_);
  loDec->escape_ = escape_;
  declarations->Prepend(loDec);

  SimpleVar *iteratorVar = new SimpleVar(pos_, var_);
  SimpleVar *limitVar = new SimpleVar(pos_, limitSymbol);
  VarExp *iteratorExp = new VarExp(pos_, iteratorVar);
  VarExp *limitExp = new VarExp(pos_, limitVar);

  OpExp *testExp = new OpExp(pos_, LE_OP, iteratorExp, limitExp);
  ExpList *bodyExps = new ExpList();
  bodyExps->Prepend(new AssignExp(
      pos_, iteratorVar,
      new OpExp(pos_, PLUS_OP, iteratorExp, new IntExp(pos_, 1))));
  bodyExps->Prepend(body_);
  SeqExp *seqExp = new SeqExp(pos_, bodyExps);

  WhileExp *whileExp = new WhileExp(pos_, testExp, seqExp);
  LetExp *letExp = new LetExp(whileExp->pos_, declarations, whileExp);

  return letExp->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  std::vector<temp::Label *> *jumps = new std::vector<temp::Label *>{label};
  tree::Stm *jumpStm = new tree::JumpStm(new tree::NameExp(label), jumps);
  return new tr::ExpAndTy(new tr::NxExp(jumpStm), type::VoidTy::Instance());
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  tenv->BeginScope();

  auto decIterator = decs_->GetList().begin();
  if (decIterator == decs_->GetList().end()) {
    tr::ExpAndTy *bodyExpTy =
        body_->Translate(venv, tenv, level, label, errormsg);
    if (typeid(*bodyExpTy->exp_) == typeid(tr::NxExp)) {
      venv->EndScope();
      tenv->EndScope();
      return new tr::ExpAndTy(bodyExpTy->exp_, type::VoidTy::Instance());
    } else {
      tree::Exp *bodyExp = bodyExpTy->exp_->UnEx();
      venv->EndScope();
      tenv->EndScope();
      return new tr::ExpAndTy(new tr::ExExp(bodyExp), bodyExpTy->ty_);
    }
  }

  tree::Stm *decStm =
      (*decIterator)->Translate(venv, tenv, level, label, errormsg)->UnNx();
  decIterator++;
  for (; decIterator != decs_->GetList().end(); decIterator++) {
    decStm = new tree::SeqStm(
        decStm,
        (*decIterator)->Translate(venv, tenv, level, label, errormsg)->UnNx());
  }

  tr::ExpAndTy *bodyExpTy =
      body_->Translate(venv, tenv, level, label, errormsg);
  if (typeid(*bodyExpTy->exp_) == typeid(tr::NxExp)) {
    tree::Stm *bodyStm = bodyExpTy->exp_->UnNx();
    tree::Stm *seqStm = new tree::SeqStm(decStm, bodyStm);
    venv->EndScope();
    tenv->EndScope();
    return new tr::ExpAndTy(new tr::NxExp(seqStm), type::VoidTy::Instance());
  } else {
    tree::Exp *bodyExp = bodyExpTy->exp_->UnEx();
    tree::Exp *eseqExp = new tree::EseqExp(decStm, bodyExp);
    venv->EndScope();
    tenv->EndScope();
    return new tr::ExpAndTy(new tr::ExExp(eseqExp), bodyExpTy->ty_);
  }
}
tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  type::Ty *typeInfo = tenv->Look(typ_);
  if (!typeInfo) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  } else if (typeid(*typeInfo->ActualTy()) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "type %s is not an array", typ_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  type::ArrayTy *arrayType = static_cast<type::ArrayTy *>(typeInfo->ActualTy());
  tr::ExpAndTy *sizeExpTy =
      size_->Translate(venv, tenv, level, label, errormsg);
  if (typeid(*sizeExpTy->ty_->ActualTy()) != typeid(type::IntTy)) {
    errormsg->Error(size_->pos_, "integer required for array size");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }
  tr::ExpAndTy *initExpTy =
      init_->Translate(venv, tenv, level, label, errormsg);
  if (!(initExpTy->ty_->IsSameType(arrayType->ty_))) {
    errormsg->Error(init_->pos_, "type mismatch");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  tree::Exp *registerExp = new tree::TempExp(temp::TempFactory::NewTemp());
  tree::ExpList *callArgs =
      new tree::ExpList({sizeExpTy->exp_->UnEx(), initExpTy->exp_->UnEx()});
  tree::Stm *initStm = new tree::MoveStm(
      registerExp, frame::CreateExternalFunctionCall("init_array", callArgs));
  tree::Exp *resultExp = new tree::EseqExp(initStm, registerExp);
  return new tr::ExpAndTy(new tr::ExExp(resultExp), arrayType);
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::VoidTy::Instance());
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  type::Ty *resultType;
  type::TyList *formalTypes;
  temp::Label *functionLabel;
  frame::Frame *newFrame;
  tr::Level *newLevel;
  std::vector<frame::Access *> formalAccesses;
  std::unordered_map<std::string, temp::Label *> functionRecord;

  for (FunDec *function : functions_->GetList()) {
    if (functionRecord.count(function->name_->Name())) {
      errormsg->Error(function->pos_, "two functions have the same name");
      return new tr::ExExp(new tree::ConstExp(0));
    }

    functionLabel = temp::LabelFactory::NewLabel();
    functionRecord[function->name_->Name()] = functionLabel;
    resultType = function->result_ ? tenv->Look(function->result_)
                                   : type::VoidTy::Instance();
    formalTypes = function->params_->MakeFormalTyList(tenv, errormsg);

    std::vector<bool> formalEscapes = std::vector<bool>{true};
    for (auto param : function->params_->GetList())
      formalEscapes.push_back(param->escape_);

    newFrame = frame::NewFrame(functionLabel, formalEscapes);
    newLevel = new tr::Level(newFrame, level);
    formalAccesses = newFrame->formalAccesses_;

    venv->Enter(function->name_, new env::FunEntry(newLevel, functionLabel,
                                                   formalTypes, resultType));
  }

  for (FunDec *function : functions_->GetList()) {
    env::EnvEntry *entry = venv->Look(function->name_);
    env::FunEntry *functionEntry = static_cast<env::FunEntry *>(entry);

    newLevel = functionEntry->level_;
    newFrame = newLevel->frame_;
    formalAccesses = newFrame->formalAccesses_;
    functionLabel = functionEntry->label_;
    resultType = function->result_ ? tenv->Look(function->result_)
                                   : type::VoidTy::Instance();
    formalTypes = function->params_->MakeFormalTyList(tenv, errormsg);

    venv->BeginScope();

    auto paramIterator = function->params_->GetList().cbegin();
    auto formalTypeIterator = formalTypes->GetList().cbegin();
    auto accessIterator =
        formalAccesses.cbegin() + 1; // The first one goes to static link
    for (; formalTypeIterator != formalTypes->GetList().cend();
         paramIterator++, formalTypeIterator++, accessIterator++) {
      venv->Enter((*paramIterator)->name_,
                  new env::VarEntry(new tr::Access(newLevel, *accessIterator),
                                    *formalTypeIterator));
    }

    tr::ExpAndTy *bodyExpTy =
        function->body_->Translate(venv, tenv, newLevel, label, errormsg);
    if (!function->result_ && typeid(*bodyExpTy->ty_) != typeid(type::VoidTy)) {
      errormsg->Error(function->body_->pos_, "procedure returns value");
    } else if (function->result_ && !(bodyExpTy->ty_->IsSameType(resultType))) {
      errormsg->Error(function->body_->pos_,
                      "return type of function %s mismatch",
                      function->name_->Name().data());
    }

    tree::Exp *result = bodyExpTy->exp_->UnEx();
    tree::Stm *bodyStm = frame::GenerateProcedureEntryExitSequence(
        newFrame, new tree::MoveStm(
                      new tree::TempExp(reg_manager->ReturnValue()), result));
    tr::ProcEntryExit(newLevel, new tr::NxExp(bodyStm));

    venv->EndScope();
  }

  return new tr::NxExp(new tree::ExpStm(new tree::ConstExp(0)));
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  type::Ty *typeInfo;
  if (typ_) {
    typeInfo = tenv->Look(typ_);
    if (!typeInfo) {
      errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
      return new tr::ExExp(new tree::ConstExp(0));
    }
  }

  tr::ExpAndTy *initExpTy =
      init_->Translate(venv, tenv, level, label, errormsg);
  tr::Access *varAccess = tr::Access::AllocLocal(level, escape_);
  env::EnvEntry *entry = new env::VarEntry(varAccess, initExpTy->ty_);
  venv->Enter(var_, entry);

  tree::Exp *accessExp =
      frame::GetCurrentAccessExpression(varAccess->access_, level->frame_);
  tree::Stm *decStm = new tree::MoveStm(accessExp, initExpTy->exp_->UnEx());

  return new tr::NxExp(decStm);
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  type::Ty *typeInfo;
  type::NameTy *tenvType;
  std::unordered_map<std::string, int> typeRecord;

  for (NameAndTy *nameAndTy : types_->GetList()) {
    if (typeRecord.count(nameAndTy->name_->Name())) {
      errormsg->Error(nameAndTy->ty_->pos_, "two types have the same name");
      return new tr::ExExp(new tree::ConstExp(0));
    }
    typeRecord[nameAndTy->name_->Name()] = 1;
    tenv->Enter(nameAndTy->name_, new type::NameTy(nameAndTy->name_, nullptr));
  }

  for (NameAndTy *nameAndTy : types_->GetList()) {
    typeInfo = tenv->Look(nameAndTy->name_);
    if (!typeInfo) {
      errormsg->Error(nameAndTy->ty_->pos_, "undefined type %s",
                      nameAndTy->name_->Name().data());
      return new tr::ExExp(new tree::ConstExp(0));
    }
    tenvType = static_cast<type::NameTy *>(typeInfo);
    tenvType->ty_ = nameAndTy->ty_->Translate(tenv, errormsg);
    tenv->Set(nameAndTy->name_, tenvType);
  }

  // Cycle detection
  for (NameAndTy *nameAndTy : types_->GetList()) {
    typeInfo = static_cast<type::NameTy *>(tenv->Look(nameAndTy->name_))->ty_;
    while (typeid(*typeInfo) == typeid(type::NameTy)) {
      tenvType = static_cast<type::NameTy *>(typeInfo);
      if (tenvType->sym_->Name() == nameAndTy->name_->Name()) {
        errormsg->Error(nameAndTy->ty_->pos_, "illegal type cycle");
        return new tr::ExExp(new tree::ConstExp(0));
      }
      typeInfo = tenvType->ty_;
    }
  }

  return new tr::NxExp(new tree::ExpStm(new tree::ConstExp(0)));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  return tenv->Look(name_);
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  type::FieldList *fieldList = record_->MakeFieldList(tenv, errormsg);
  return new type::RecordTy(fieldList);
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  return new type::ArrayTy(tenv->Look(array_));
}

} // namespace absyn
