#include "straightline/slp.h"

#include <iostream>

namespace A {
int A::CompoundStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  int result = std::max(this->stm1->MaxArgs(), this->stm2->MaxArgs());
  return result;
}

Table *A::CompoundStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  Table *stm1Table = this->stm1->Interp(t);
  Table *stm2Table = this->stm2->Interp(stm1Table);
  return (stm2Table);
}

int A::AssignStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  int result = exp->MaxArgs();
  return result;
}

Table *A::AssignStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable *expNode = this->exp->Interp(t);
  Table *result = expNode->t->Update(id, expNode->i);
  return (result);
}

int A::PrintStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  int result = std::max(this->exps->NumExps(), this->exps->MaxArgs());
  return result;
}

Table *A::PrintStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  Table *resultTable = t;
  ExpList *focus = this->exps;
  while (1) {
    IntAndTable *obj = focus->Interp(resultTable);
    resultTable = obj->t;
    std::cout << obj->i;

    focus = focus->getNext();
    if (focus == nullptr) {
      std::cout << '\n';
      break;
    } else {
      std::cout << ' ';
    }
  }
  return (resultTable);
}

int A::IdExp::MaxArgs() const { return 0; }

IntAndTable *A::IdExp::Interp(Table *t) const {
  return new IntAndTable(t->Lookup(id), t);
}

int A::NumExp::MaxArgs() const { return 0; }

IntAndTable *A::NumExp::Interp(Table *t) const {
  return new IntAndTable(this->num, t);
}

int A::OpExp::MaxArgs() const {
  int result = std::max(this->left->MaxArgs(), this->right->MaxArgs());
  return result;
}

IntAndTable *A::OpExp::Interp(Table *t) const {
  IntAndTable *leftNode = this->left->Interp(t);
  IntAndTable *rightNode = this->right->Interp(leftNode->t);
  int resultNum;
  switch (this->oper) {
  case BinOp::PLUS: {
    resultNum = leftNode->i + rightNode->i;
    break;
  }
  case BinOp::MINUS: {
    resultNum = leftNode->i - rightNode->i;
    break;
  }
  case BinOp::TIMES: {
    resultNum = leftNode->i * rightNode->i;
    break;
  }
  case BinOp::DIV: {
    if (rightNode->i == 0)
      assert(false);
    resultNum = leftNode->i / rightNode->i;
    break;
  }
  }
  IntAndTable *result = new IntAndTable(resultNum, rightNode->t);
  delete leftNode;
  delete rightNode;
  return result;
}

int A::EseqExp::MaxArgs() const {
  int result = std::max(this->stm->MaxArgs(), this->exp->MaxArgs());
  return result;
}

IntAndTable *A::EseqExp::Interp(Table *t) const {
  Table *stmTable = this->stm->Interp(t);
  IntAndTable *result = this->exp->Interp(stmTable);
  return result;
}

int A::PairExpList::MaxArgs() const {
  int result = std::max(this->exp->MaxArgs(), this->tail->MaxArgs());
  return result;
}

int A::PairExpList::NumExps() const { return 1 + this->tail->NumExps(); }

IntAndTable *A::PairExpList::Interp(Table *t) const {
  return this->exp->Interp(t);
}

ExpList *A::PairExpList::getNext() const { return (this->tail); }

int A::LastExpList::MaxArgs() const {
  int result = this->exp->MaxArgs();
  return result;
}

int A::LastExpList::NumExps() const { return 1; }

IntAndTable *A::LastExpList::Interp(Table *t) const {
  return this->exp->Interp(t);
}

ExpList *A::LastExpList::getNext() const { return (nullptr); }

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
  Table *focusTable = (Table *)this;
  while (focusTable != nullptr) {
    if (key == focusTable->id) {
      focusTable->value = val;
      return (Table *)this;
    }
    focusTable = (Table *)(focusTable->tail);
  }
  return new Table(key, val, this);
}
} // namespace A
