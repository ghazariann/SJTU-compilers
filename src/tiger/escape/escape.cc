#include "tiger/escape/escape.h"
#include "tiger/absyn/absyn.h"

namespace esc {
void EscFinder::FindEscape() { absyn_tree_->Traverse(env_.get()); }
} // namespace esc

namespace absyn {

void AbsynTree::Traverse(esc::EscEnvPtr env) {
  root_->Traverse(env, 0);  // Traverse the entire abstract syntax tree
}

void SimpleVar::Traverse(esc::EscEnvPtr env, int depth) {
  // If the variable is accessed at a depth greater than where it was defined,
            // it is marked as escaping.
  esc::EscapeEntry *e = env->Look(sym_);
  if (e != nullptr && e->depth_ < depth) {
    *(e->escape_) = true;
  }
  // let var a := 10 in (let var b := a in end) end, `a` escapes
}

void FieldVar::Traverse(esc::EscEnvPtr env, int depth) {
    // Traverse the variable that this field is a part of.
    var_->Traverse(env, depth);
    // let var r := {x=0, y=1} in r.x end`, `r.x` is traversed.

}

void SubscriptVar::Traverse(esc::EscEnvPtr env, int depth) {
  // Traverse the variable that this field is a part of.
  var_->Traverse(env, depth);
  subscript_->Traverse(env, depth);
  // let var arr := array [10] of 0 in arr[0] end`, `arr` and `0` are traversed.
}

void VarExp::Traverse(esc::EscEnvPtr env, int depth) {
    var_->Traverse(env, depth);
    // let var a := 10 in a end`, `a` is traversed
}

void NilExp::Traverse(esc::EscEnvPtr env, int depth) {
  // no need to traverse
}

void IntExp::Traverse(esc::EscEnvPtr env, int depth) {
  // no need to traverse
}

void StringExp::Traverse(esc::EscEnvPtr env, int depth) {
  // no need to traverse
}

void CallExp::Traverse(esc::EscEnvPtr env, int depth) {
   // Traverse each argument in the function call.
   for (Exp * arg : args_->GetList())
    arg->Traverse(env, depth); // `foo(1, 2)`, both `1` and `2` are traversed.
}

void OpExp::Traverse(esc::EscEnvPtr env, int depth) {
  // Traverse both the left and right operands of the operator.
  left_->Traverse(env, depth);
  right_->Traverse(env, depth);
}

void RecordExp::Traverse(esc::EscEnvPtr env, int depth) {
  //Traverse each field in the record expression.
    for (EField * f : fields_->GetList()) 
        f->exp_->Traverse(env, depth);
    // In `{x=1, y=2}`, both `1` and `2` are traversed
}

void SeqExp::Traverse(esc::EscEnvPtr env, int depth) {
  // Traverse each expression in the sequence.
  for (Exp * e : seq_->GetList())
    e->Traverse(env, depth);
    //In `(1; 2; 3)`, `1`, `2`, and `3` are traversed.
}

void AssignExp::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
  exp_->Traverse(env, depth);
  // In `a := 10`, `a` and `10` are traversed.
}

void IfExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
    test_->Traverse(env, depth);
  then_->Traverse(env, depth);
  if (elsee_)
    elsee_->Traverse(env, depth);
}

void WhileExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
    test_->Traverse(env, depth);
  body_->Traverse(env, depth);
}

void ForExp::Traverse(esc::EscEnvPtr env, int depth) {
  // Traverse the lower and upper bounds of the for loop.
  lo_->Traverse(env, depth);
  hi_->Traverse(env, depth);

  env->BeginScope();
  escape_ = false;
  env->Enter(var_, new esc::EscapeEntry(depth, &escape_));
  body_->Traverse(env, depth);
  env->EndScope();
  //for i := lo to hi do body`, `lo`, `hi`, and `body` are traversed.
}

void BreakExp::Traverse(esc::EscEnvPtr env, int depth) {
  // no need to traverse
}

void LetExp::Traverse(esc::EscEnvPtr env, int depth) {
  // Traverse each declaration in the let expression.
  env->BeginScope();
  for (Dec * d : decs_->GetList())
    d->Traverse(env, depth);
  body_->Traverse(env, depth);
  env->EndScope();
}

void ArrayExp::Traverse(esc::EscEnvPtr env, int depth) {
  size_->Traverse(env, depth);
  init_->Traverse(env, depth);
}

void VoidExp::Traverse(esc::EscEnvPtr env, int depth) {
  // no need to traverse
}

void FunctionDec::Traverse(esc::EscEnvPtr env, int depth) {
  // Traverse each function in the function declaration.
  for (FunDec * f : functions_->GetList()) {
    env->BeginScope();
    for (Field * param : f->params_->GetList()) {
      param->escape_ = false;
      env->Enter(param->name_, new esc::EscapeEntry(depth + 1, &(param->escape_)));
    }
    f->body_->Traverse(env, depth + 1);
    env->EndScope();
  }
}

void VarDec::Traverse(esc::EscEnvPtr env, int depth) {
  // Traverse the initial value of the variable.
  escape_ = false;
  env->Enter(var_, new esc::EscapeEntry(depth, &escape_));
  init_->Traverse(env, depth);
}

void TypeDec::Traverse(esc::EscEnvPtr env, int depth) {
  // no need to traverse
}

} // namespace absyn
