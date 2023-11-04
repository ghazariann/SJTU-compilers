#include "tiger/semant/semant.h"
#include "tiger/absyn/absyn.h"
#include <unordered_set>
using namespace std;
namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  root_->SemAnalyze(venv, tenv, 0, errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  env::EnvEntry *var = venv->Look(sym_);
  if (!var || typeid(*var) != typeid(env::VarEntry)) {
    errormsg->Error(pos_, "undefined variable %s",
                    sym_->Name().data()); // test19 20
    return type::IntTy::Instance();
  }
  return (static_cast<env::VarEntry *>(var))->ty_->ActualTy();
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *varType =
      var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*varType) != typeid(type::RecordTy)) {
    errormsg->Error(var_->pos_, "not a record type"); // test25
    return type::IntTy::Instance();
  }
  type::FieldList *fieldList = static_cast<type::RecordTy *>(varType)->fields_;
  for (type::Field *f : fieldList->GetList()) {
    if (f->name_->Name() == sym_->Name()) {
      return f->ty_->ActualTy();
    }
  }

  errormsg->Error(pos_, "field %s doesn't exist",
                  sym_->Name().data()); // test22
  return type::IntTy::Instance();
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {

  type::Ty *varActualType =
      var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *subscriptActualType =
      subscript_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  //  if the variable is of Array type
  if (typeid(*varActualType) != typeid(type::ArrayTy)) {
    errormsg->Error(var_->pos_, "array type required");
    return type::IntTy::Instance();
  }

  // if the subscript is of Int type
  if (typeid(*subscriptActualType) != typeid(type::IntTy)) {
    errormsg->Error(subscript_->pos_, "ARRAY can only be subscripted by INT.");
    return type::IntTy::Instance();
  }

  return static_cast<type::ArrayTy *>(varActualType)->ty_;
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {

  env::EnvEntry *entry = venv->Look(func_);
  if (!entry || typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return type::VoidTy::Instance();
  }

  env::FunEntry *funcEntry = static_cast<env::FunEntry *>(entry);

  const auto &argumentsList = args_->GetList();
  const auto &formalsList = funcEntry->formals_->GetList();

  auto argIt = argumentsList.begin();
  auto formalIt = formalsList.begin();

  while (argIt != argumentsList.end() && formalIt != formalsList.end()) {
    type::Ty *argumentType =
        (*argIt)->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
    type::Ty *formalType = (*formalIt)->ActualTy();

    if (!argumentType->IsSameType(formalType)) {
      errormsg->Error((*argIt)->pos_, "para type mismatch"); // test34 35
      return type::VoidTy::Instance();
    }

    argIt++;
    formalIt++;
  }

  if (argIt != argumentsList.end() || formalIt != formalsList.end()) {
    int errorPosition = argIt == argumentsList.end()
                            ? argumentsList.back()->pos_
                            : (*argIt)->pos_;
    errormsg->Error(errorPosition, "too many params in function %s",
                    func_->Name().data()); // test36
    return type::VoidTy::Instance();
  }

  if (!funcEntry->result_)
    return type::VoidTy::Instance();

  return funcEntry->result_->ActualTy();
}
type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *leftType =
      left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *rightType =
      right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (oper_ == absyn::PLUS_OP || oper_ == absyn::MINUS_OP ||
      oper_ == absyn::TIMES_OP || oper_ == absyn::DIVIDE_OP) {
    if (typeid(*leftType) != typeid(type::IntTy) ||
        typeid(*rightType) != typeid(type::IntTy)) {
      errormsg->Error(left_->pos_, "integer required"); // test26
    }
  } else if (!leftType->IsSameType(rightType)) {
    errormsg->Error(left_->pos_, "same type required"); // test13 14
  }

  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *type = tenv->Look(typ_);
  if (!type) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return type::VoidTy::Instance();
  }
  return type;
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *resultType;
  for (Exp *expression : seq_->GetList()) {
    resultType = expression->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  return resultType;
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *variableType =
      var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *expressionType =
      exp_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (!variableType->IsSameType(expressionType)) {
    errormsg->Error(exp_->pos_, "unmatched assign exp"); // test23
    return type::VoidTy::Instance();
  }

  if (typeid(*var_) == typeid(SimpleVar)) {
    SimpleVar *simpleVariable = static_cast<SimpleVar *>(var_);
    if (venv->Look(simpleVariable->sym_)->readonly_) {
      errormsg->Error(var_->pos_, "loop variable can't be assigned"); // test11
      return type::VoidTy::Instance();
    }
  }

  return type::VoidTy::Instance();
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *testType =
      test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *thenType =
      then_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *elseType =
      elsee_ ? elsee_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy()
             : nullptr;

  if (!elsee_ && typeid(*thenType) != typeid(type::VoidTy)) {
    errormsg->Error(then_->pos_,
                    "if-then exp's body must produce no value"); // test15
    return type::VoidTy::Instance();
  }

  if (elsee_ && !thenType->IsSameType(elseType)) {
    errormsg->Error(elsee_->pos_,
                    "then exp and else exp type mismatch"); // test9
    return thenType;
  }

  return thenType;
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  tenv->BeginScope();

  type::Ty *bodyType = body_->SemAnalyze(venv, tenv, -1, errormsg)->ActualTy();
  if (typeid(*bodyType) != typeid(type::VoidTy)) {
    errormsg->Error(body_->pos_, "while body must produce no value"); // test10
    return type::VoidTy::Instance();
  }

  venv->EndScope();
  tenv->EndScope();

  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount,
                             err::ErrorMsg *errorMessages) const {
  type::Ty *lowerBoundType, *upperBoundType, *loopBodyType;

  venv->BeginScope();
  tenv->BeginScope();

  venv->Enter(var_, new env::VarEntry(type::IntTy::Instance(), true));

  lowerBoundType =
      lo_->SemAnalyze(venv, tenv, labelcount, errorMessages)->ActualTy();
  if (typeid(*lowerBoundType) != typeid(type::IntTy)) {
    errorMessages->Error(lo_->pos_,
                         "for exp's range type is not integer"); // test11
  }

  upperBoundType =
      hi_->SemAnalyze(venv, tenv, labelcount, errorMessages)->ActualTy();
  if (typeid(*upperBoundType) != typeid(type::IntTy)) {
    errorMessages->Error(hi_->pos_,
                         "for exp's range type is not integer"); // test11
  }

  loopBodyType = body_->SemAnalyze(venv, tenv, -1, errorMessages)->ActualTy();

  venv->EndScope();
  tenv->EndScope();

  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  if (labelcount != -1) {
    errormsg->Error(pos_, "break is not inside any loop"); // test50
  }
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  tenv->BeginScope();

  for (Dec *declaration : decs_->GetList()) {
    declaration->SemAnalyze(venv, tenv, labelcount, errormsg);
  }

  type::Ty *bodyType =
      body_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  venv->EndScope();
  tenv->EndScope();

  return bodyType;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *arrayType = tenv->Look(typ_);

  type::Ty *sizeType = size_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *initType = init_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (initType->ActualTy() !=
      static_cast<type::ArrayTy *>(arrayType->ActualTy())->ty_->ActualTy()) {
    errormsg->Error(init_->pos_, "type mismatch"); // test23
  }

  return arrayType;
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {

  type::TyList *parametersTypeList;
  type::Ty *returnType, *functionBodyType;
  std::unordered_set<string> functionNameRecord;

  const auto &functionList = functions_->GetList();

  for (FunDec *function : functionList) {
    if (!functionNameRecord.insert(function->name_->Name()).second) {
      errormsg->Error(function->pos_,
                      "two functions have the same name"); // test39
      return;
    }
    returnType = function->result_ ? tenv->Look(function->result_) : nullptr;
    parametersTypeList = function->params_->MakeFormalTyList(tenv, errormsg);
    venv->Enter(function->name_,
                new env::FunEntry(parametersTypeList, returnType));
  }

  for (FunDec *functionDeclaration : functionList) {
    returnType = functionDeclaration->result_
                     ? tenv->Look(functionDeclaration->result_)
                     : nullptr;
    parametersTypeList =
        functionDeclaration->params_->MakeFormalTyList(tenv, errormsg);

    venv->BeginScope();

    auto paramsIterator = functionDeclaration->params_->GetList().begin();
    auto formalTypesIterator = parametersTypeList->GetList().begin();
    for (; formalTypesIterator != parametersTypeList->GetList().end();
         paramsIterator++, formalTypesIterator++) {
      venv->Enter((*paramsIterator)->name_,
                  new env::VarEntry(*formalTypesIterator));
    }

    functionBodyType = functionDeclaration->body_->SemAnalyze(
        venv, tenv, labelcount, errormsg);
    if (!functionDeclaration->result_ &&
        typeid(*functionBodyType) != typeid(type::VoidTy)) {
      errormsg->Error(functionDeclaration->body_->pos_,
                      "procedure returns value"); // test40
    }
    venv->EndScope();
  }
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {

  type::Ty *specifiedType, *initializerType;

  if (typ_) {
    specifiedType = tenv->Look(typ_);
    if (!specifiedType) {
      errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
      return;
    }
  }

  initializerType = init_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (typeid(*initializerType) == typeid(type::NilTy) &&
      (!typ_ || typeid(*specifiedType->ActualTy()) != typeid(type::RecordTy))) {
    errormsg->Error(init_->pos_,
                    "init should not be nil without type specified"); // test 45
    return;
  } else if (typ_ && !(initializerType->IsSameType(specifiedType))) {
    errormsg->Error(init_->pos_, "type mismatch"); // test23
    return;
  }

  venv->Enter(var_, new env::VarEntry(initializerType));
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errorMessages) const {

  type::Ty *currentTypeDefinition;
  type::NameTy *typeFromEnvironment;
  std::unordered_set<string> typeValidationSet;

  const std::list<NameAndTy *> &typeDeclarationList = types_->GetList();

  // duplicate type names
  for (NameAndTy *typeEntry : typeDeclarationList) {
    if (typeValidationSet.count(typeEntry->name_->Name())) {
      errorMessages->Error(typeEntry->ty_->pos_,
                           "two types have the same name"); // test38
      return;
    }
    typeValidationSet.insert(typeEntry->name_->Name());
    tenv->Enter(typeEntry->name_, new type::NameTy(typeEntry->name_, nullptr));
  }

  // update type definitions
  for (NameAndTy *typeEntry : typeDeclarationList) {
    currentTypeDefinition = tenv->Look(typeEntry->name_);
    if (!currentTypeDefinition) {
      errorMessages->Error(typeEntry->ty_->pos_, "undefined type %s",
                           typeEntry->name_->Name().data()); // test17
      return;
    }
    typeFromEnvironment = static_cast<type::NameTy *>(currentTypeDefinition);
    typeFromEnvironment->ty_ = typeEntry->ty_->SemAnalyze(tenv, errorMessages);
    tenv->Set(typeEntry->name_, typeFromEnvironment);
  }

  // cyclic type
  for (NameAndTy *typeEntry : types_->GetList()) {
    currentTypeDefinition =
        static_cast<type::NameTy *>(tenv->Look(typeEntry->name_))->ty_;
    while (typeid(*currentTypeDefinition) == typeid(type::NameTy)) {
      typeFromEnvironment = static_cast<type::NameTy *>(currentTypeDefinition);
      if (typeFromEnvironment->sym_->Name() == typeEntry->name_->Name()) {
        errorMessages->Error(typeEntry->ty_->pos_,
                             "illegal type cycle"); // test16
        return;
      }
      currentTypeDefinition = typeFromEnvironment->ty_;
    }
  }
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(name_);
  return ty;
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  type::FieldList *fieldList = record_->MakeFieldList(tenv, errormsg);
  return new type::RecordTy(fieldList);
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  return new type::ArrayTy(tenv->Look(array_));
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}

} // namespace sem
