#include "straightline/slp.h"
#include <algorithm>
#include <iostream>

namespace A {
int A::CompoundStm::MaxArgs() const {
  return std::max(stm1->MaxArgs(), stm2->MaxArgs());
}

Table *A::CompoundStm::Interp(Table *t) const {
  return stm2->Interp(stm1->Interp(t)); // first update stm1 then stm2
}

int A::AssignStm::MaxArgs() const { return exp->MaxArgs(); }

Table *A::AssignStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable *intAndTable = exp->InterpExp(t);
  Table *tableToUpdate = intAndTable->t;
  int newVal = intAndTable->i; // new value for id to assign
  Table *updatedTable = tableToUpdate->Update(id, newVal); // new val from front
  return updatedTable;
}

int A::PrintStm::MaxArgs() const { return exps->MaxArgs(); }
Table *A::PrintStm::Interp(Table *t) const { return exps->InterpExp(t)->t; }

int A::NumExp::MaxArgs() const { return 1; }
IntAndTable *A::NumExp::InterpExp(Table *t) const {
  return new IntAndTable(this->num, t);
};
int A::IdExp::MaxArgs() const { return 1; }
IntAndTable *A::IdExp::InterpExp(Table *t) const {
  int num = t->Lookup(this->id);
  return new IntAndTable(num, t);
};

int A::OpExp::MaxArgs() const { return 1; }
IntAndTable *A::OpExp::InterpExp(Table *t) const {
  IntAndTable *leftAndTable = left->InterpExp(t);
  IntAndTable *rightAndTable = right->InterpExp(leftAndTable->t);
  switch (oper) {
  case PLUS:
    return new IntAndTable(leftAndTable->i + rightAndTable->i,
                           rightAndTable->t);
  case MINUS:
    return new IntAndTable(leftAndTable->i - rightAndTable->i,
                           rightAndTable->t);
  case TIMES:
    return new IntAndTable(leftAndTable->i * rightAndTable->i,
                           rightAndTable->t);
  case DIV:
    return new IntAndTable(leftAndTable->i / rightAndTable->i,
                           rightAndTable->t);
  default:
    break;
  }
};

// like compoundStm (stm, exp)
int A::EseqExp::MaxArgs() const {
  return std::max(stm->MaxArgs(), exp->MaxArgs());
}
IntAndTable *A::EseqExp::InterpExp(Table *t) const {
  return exp->InterpExp(stm->Interp(t)); // first stm then expression
}

// or 1 cause exp->MaxArgs() = 1
int A::PairExpList::MaxArgs() const { return exp->MaxArgs() + tail->MaxArgs(); }
IntAndTable *A::PairExpList::InterpExp(Table *t) const {
  IntAndTable *lastExpAndTable = exp->InterpExp(t);
  printf("%d ", lastExpAndTable->i);
  return tail->InterpExp(lastExpAndTable->t);
}
int A::LastExpList::MaxArgs() const { return exp->MaxArgs(); }
IntAndTable *A::LastExpList::InterpExp(Table *t) const { // need to print
  IntAndTable *lastExpAndTable = exp->InterpExp(t);
  printf("%d\n", lastExpAndTable->i);
  return lastExpAndTable;
}

int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}
} // namespace A
