#include "tiger/escape/escape.h"
#include "tiger/absyn/absyn.h"

namespace esc {
void EscFinder::FindEscape() { absyn_tree_->Traverse(env_.get()); }
} // namespace esc

namespace absyn {

void AbsynTree::Traverse(esc::EscEnvPtr env) {
  /* TODO: Put your lab5 code here */
  this->root_->Traverse(env, 0);
}

// 检验当前的变量所处的深度是否比EscapeEntry中的深度更大
// 如果更大则代表其被深层的函数使用，是逃逸的
void SimpleVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  auto escape_item = env->Look(this->sym_);
  if (escape_item == nullptr) {
    // FIXME: how to handle error
    printf("error: %s not found\n", sym_->Name().c_str());
    return;
  }
  if (depth > escape_item->depth_) {
    *(escape_item->escape_) = true;
  }
}

// 只对记录变量做逃逸分析即可，其中的field不需要做
void FieldVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->var_->Traverse(env, depth);
}

// 只对数组变量做逃逸分析即可，其中的field不需要做
void SubscriptVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->var_->Traverse(env, depth);
}

// 对其中的var语句进行逃逸分析即可
void VarExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->var_->Traverse(env, depth);
}

// 空语句不需要逃逸分析
void NilExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

// 数字语句不需要逃逸分析
void IntExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

// 字符串语句不需要逃逸分析
void StringExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

// 函数调用语句，需要将其参数示例全部做一边逃逸分析
void CallExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  auto argument_list = this->args_->GetList();
  auto it = argument_list.begin();
  auto end = argument_list.end();
  for (; it != end; ++it) {
    (*it)->Traverse(env, depth);
  }
}

// 二元运算操作符需要左右的语句做逃逸分析
void OpExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->left_->Traverse(env, depth);
  this->right_->Traverse(env, depth);
}

// 一个记录的实例，需要其中的所有field的实例需要做逃逸分析
void RecordExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  auto efield_list = this->fields_->GetList();
  for (EField *item : efield_list) {
    item->exp_->Traverse(env, depth);
  }
}

// 语句序列依次做逃逸分析
void SeqExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  auto exp_list = this->seq_->GetList();
  for (Exp *item : exp_list) {
    item->Traverse(env, depth);
  }
}

// 赋值语句的变量和值语句都要进行逃逸分析
void AssignExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->var_->Traverse(env, depth);
  this->exp_->Traverse(env, depth);
}

// if语句，三个语句依次做逃逸分析即可
void IfExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->test_->Traverse(env, depth);
  this->then_->Traverse(env, depth);
  if (this->elsee_ != nullptr)
    this->elsee_->Traverse(env, depth);
}

// while语句，所有语句依次做逃逸分析即可
void WhileExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->test_->Traverse(env, depth);
  this->body_->Traverse(env, depth);
}

// for语句，所有的语句都做一次逃逸分析
// for中定义了一个循环体中只读的循环变量，这个变量有可能逃逸，，故需要创建新的EscapeEntry
void ForExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->escape_ = false;
  env->Enter(this->var_, new esc::EscapeEntry(depth, &(this->escape_)));
  this->lo_->Traverse(env, depth);
  this->hi_->Traverse(env, depth);
  this->body_->Traverse(env, depth);
}

// break语句不需要逃逸分析
void BreakExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

// let in语句，先对所有的定义做逃逸分析，后对body逃逸分析
void LetExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  auto dec_list = this->decs_->GetList();
  for (Dec *item : dec_list) {
    item->Traverse(env, depth);
  }
  this->body_->Traverse(env, depth);
}

// 数组初始化语句，对所有语句进行逃逸分析即可
void ArrayExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->size_->Traverse(env, depth);
  this->init_->Traverse(env, depth);
}

// Void语句不需要逃逸分析
void VoidExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

// 函数定义语句
// 在新的函数中，depth+1,此时如果再调用非本函数定义外的内容，则满足逃逸分析条件
void FunctionDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  // 函数深度+1
  int new_depth = depth + 1;
  auto fun_dec_list = this->functions_->GetList();
  // 对每个函数声明进行逃逸分析
  for (FunDec *item : fun_dec_list) {
    // 参数列表加入EscapeEntry（不确定参数是否也会逃逸）
    auto field_list = item->params_->GetList();
    for (Field *f_item : field_list) {
      f_item->escape_ = false;
      env->Enter(f_item->name_,
                 new esc::EscapeEntry(new_depth, &(f_item->escape_)));
    }
    // 函数体逃逸分析
    item->body_->Traverse(env, new_depth);
  }
}

// 变量声明语句，将变量加入EscapeEntry中，初始值语句要进行逃逸分析
void VarDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  init_->Traverse(env, depth);
  this->escape_ = false;
  env->Enter(this->var_, new esc::EscapeEntry(depth, &(this->escape_)));
}

void TypeDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

} // namespace absyn
