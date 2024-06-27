#ifndef STRAIGHTLINE_SLP_H_
#define STRAIGHTLINE_SLP_H_

#include <algorithm>
#include <cassert>
#include <list>
#include <string>

namespace A {

class Stm;
class Exp;
class ExpList;

enum BinOp { PLUS = 0, MINUS, TIMES, DIV };

// some data structures used by interp
class Table;
class IntAndTable;

class Stm {
public:
  virtual int MaxArgs() const = 0;
  virtual Table *Interp(Table *) const = 0;
};

// 复合语句节点，可以拆出一个其他的语句+一个子复合语句
class CompoundStm : public Stm {
public:
  CompoundStm(Stm *stm1, Stm *stm2) : stm1(stm1), stm2(stm2) {}
  int MaxArgs() const override;
  Table *Interp(Table *) const override;

private:
  Stm *stm1, *stm2;
};

// 赋值语句，将一个节点的值赋值给另一个节点的变量
class AssignStm : public Stm {
public:
  AssignStm(std::string id, Exp *exp) : id(std::move(id)), exp(exp) {}
  int MaxArgs() const override;
  Table *Interp(Table *) const override;

private:
  std::string id;
  Exp *exp;
};

// 打印语句，逐个打印语句的子语句（参数）
class PrintStm : public Stm {
public:
  explicit PrintStm(ExpList *exps) : exps(exps) {}
  int MaxArgs() const override;
  Table *Interp(Table *) const override;

private:
  ExpList *exps;
};

class Exp {
  // TODO: you'll have to add some definitions here (lab1).
  // Hints: You may add interfaces like `int MaxArgs()`,
  //        and ` IntAndTable *Interp(Table *)`
public:
  virtual int MaxArgs() const = 0;
  virtual IntAndTable *Interp(Table *) const = 0;
};

// 标记一个符号ID的语句
class IdExp : public Exp {
public:
  explicit IdExp(std::string id) : id(std::move(id)) {}
  // TODO: you'll have to add some definitions here (lab1).
  int MaxArgs() const override;
  IntAndTable *Interp(Table *) const override;

private:
  std::string id;
};

// 数字语句，指定一个数字
class NumExp : public Exp {
public:
  explicit NumExp(int num) : num(num) {}
  // TODO: you'll have to add some definitions here.
  int MaxArgs() const override;
  IntAndTable *Interp(Table *) const override;

private:
  int num;
};

// 二元运算语句，指定运算符和左右运算数，并可以计算运算结果
class OpExp : public Exp {
public:
  OpExp(Exp *left, BinOp oper, Exp *right)
      : left(left), oper(oper), right(right) {}
  int MaxArgs() const override;
  IntAndTable *Interp(Table *) const override;

private:
  Exp *left;
  BinOp oper;
  Exp *right;
};

// 语句序列，可以拆出一个实际语句和一个子语句序列
class EseqExp : public Exp {
public:
  EseqExp(Stm *stm, Exp *exp) : stm(stm), exp(exp) {}
  int MaxArgs() const override;
  IntAndTable *Interp(Table *) const override;

private:
  Stm *stm;
  Exp *exp;
};

class ExpList {
  // TODO: you'll have to add some definitions here (lab1).
  // Hints: You may add interfaces like `int MaxArgs()`, `int NumExps()`,
  //        and ` IntAndTable *Interp(Table *)`
public:
  virtual int MaxArgs() const = 0;
  virtual int NumExps() const = 0;
  virtual IntAndTable *Interp(Table *) const = 0;
  virtual ExpList *getNext() const = 0;
};

// 中间的ExpList,可以拆出实际的语句
class PairExpList : public ExpList {
public:
  PairExpList(Exp *exp, ExpList *tail) : exp(exp), tail(tail) {}
  // TODO: you'll have to add some definitions here (lab1).
  int MaxArgs() const override;
  int NumExps() const override;
  IntAndTable *Interp(Table *) const override;
  ExpList *getNext() const override;

private:
  Exp *exp;
  ExpList *tail;
};

// 终止的ExpList,其如果代表一个语句序列的终止
class LastExpList : public ExpList {
public:
  LastExpList(Exp *exp) : exp(exp) {}
  // TODO: you'll have to add some definitions here (lab1).
  int MaxArgs() const override;
  int NumExps() const override;
  IntAndTable *Interp(Table *) const override;
  ExpList *getNext() const override;

private:
  Exp *exp;
};

// 记录扫描到的id-value键值对，并组成一个链表供后续查阅
class Table {
public:
  Table(std::string id, int value, const Table *tail)
      : id(std::move(id)), value(value), tail(tail) {}
  int Lookup(const std::string &key) const;
  Table *Update(const std::string &key, int val) const;

private:
  std::string id;
  int value;
  const Table *tail;
};

struct IntAndTable {
  int i;
  Table *t;

  IntAndTable(int i, Table *t) : i(i), t(t) {}
};

} // namespace A

#endif // STRAIGHTLINE_SLP_H_
