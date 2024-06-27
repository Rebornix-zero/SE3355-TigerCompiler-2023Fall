%filenames parser
%scanner tiger/lex/scanner.h
%baseclass-preinclude tiger/absyn/absyn.h

 /*
  * Please don't modify the lines above.
  */

%union {
  int ival;
  std::string* sval;
  sym::Symbol *sym;
  absyn::Exp *exp;
  absyn::ExpList *explist;
  absyn::Var *var;
  absyn::DecList *declist;
  absyn::Dec *dec;
  absyn::EFieldList *efieldlist;
  absyn::EField *efield;
  absyn::NameAndTyList *tydeclist;
  absyn::NameAndTy *tydec;
  absyn::FieldList *fieldlist;
  absyn::Field *field;
  absyn::FunDecList *fundeclist;
  absyn::FunDec *fundec;
  absyn::Ty *ty;
  }

%token <sym> ID
%token <sval> STRING
%token <ival> INT

/*
    标点符号（Punctuation）：
        COMMA：逗号，通常用于分隔变量或参数。
        COLON：冒号，通常用于类型注解或分隔键值对中的键和值。
        SEMICOLON：分号，通常用于分隔语句。
        LPAREN 和 RPAREN：左括号和右括号，用于括号表达式。
        LBRACK 和 RBRACK：左方括号和右方括号，用于定义数组或访问数组元素。
        LBRACE 和 RBRACE：左大括号和右大括号，通常用于定义块作用域或对象字面量。
        DOT：点号，通常用于访问对象的属性。

    算术运算符（Arithmetic Operators）：
        PLUS：加法运算符。
        MINUS：减法运算符。
        TIMES：乘法运算符。
        DIVIDE：除法运算符。

    比较运算符（Comparison Operators）：
        EQ：等于。
        NEQ：不等于。
        LT：小于。
        LE：小于等于。
        GT：大于。
        GE：大于等于。

    逻辑运算符（Logical Operators）：
        AND：逻辑与。
        OR：逻辑或。

    赋值运算符（Assignment Operator）：
        ASSIGN：赋值操作符，用于将一个值分配给一个变量。

    关键字（Keywords）：
        这些是编程语言中的保留字，具有特殊的含义，通常用于控制流、声明、定义和其他语言结构。

    其他 Tokens：
        ARRAY：用于声明数组类型。
        IF、THEN、ELSE：用于条件语句。
        WHILE：用于循环语句。
        FOR、TO、DO：用于循环控制。
        LET、IN、END：通常与变量绑定和作用域相关。
        BREAK：用于中断循环。
        NIL：表示空值。
        FUNCTION：用于声明函数。
        VAR：用于声明变量。
        TYPE：用于声明类型。
*/

%token
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK
  LBRACE RBRACE DOT
  ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF
  BREAK NIL
  FUNCTION VAR TYPE

 /* token priority */
 /* TODO: Put your lab3 code here */
%right ASSIGN
%left OR
%left AND
%nonassoc EQ NEQ LT LE GT GE
%left PLUS MINUS
%left TIMES DIVIDE
%left UMINUS


%type <exp> exp expseq
%type <explist> actuals nonemptyactuals sequencing sequencing_exps
%type <var> lvalue one oneormore
%type <declist> decs decs_nonempty
%type <dec> decs_nonempty_s vardec
%type <efieldlist> rec rec_nonempty
%type <efield> rec_one
%type <tydeclist> tydec
%type <tydec> tydec_one
%type <fieldlist> tyfields tyfields_nonempty
%type <field> tyfield
%type <ty> ty
%type <fundeclist> fundec
%type <fundec> fundec_one

// 此语句是归约最后的终点
%start program

%%
/*  START CFG  */
program:  
    exp {absyn_tree_ = std::make_unique<absyn::AbsynTree>($1);}
;

/********************/
/*  DEFINATION CFG  */
/********************/
decs:{ $$ = new absyn::DecList();}
|   decs_nonempty { $$ = ($1); }
;

decs_nonempty:
    decs_nonempty_s { $$ = new absyn::DecList($1);}
|   decs_nonempty_s decs_nonempty { $$ = ($2)->Prepend($1);}
;

decs_nonempty_s:
    vardec { $$=($1); }
|   tydec { $$=new absyn::TypeDec(scanner_.GetTokPos(),$1);}
|   fundec {$$=new absyn::FunctionDec(scanner_.GetTokPos(),$1);}
;

/*  DEFINATION CFG : function define */
fundec:
    fundec_one {$$=new absyn::FunDecList($1);}
|   fundec_one fundec{$$=($2)->Prepend($1);}
;

fundec_one:
    FUNCTION ID LPAREN tyfields RPAREN COLON ID EQ exp{
        $$=new absyn::FunDec(scanner_.GetTokPos(),$2,$4,$7,$9);
    }
|   FUNCTION ID LPAREN tyfields RPAREN EQ exp{
        $$=new absyn::FunDec(scanner_.GetTokPos(),$2,$4,nullptr,$7);
    }
;

/*  DEFINATION CFG : type define */
tydec:
    tydec_one { $$=new absyn::NameAndTyList($1); }
|   tydec_one tydec { $$=($2)->Prepend($1); }
;

tydec_one:
    TYPE ID EQ ty { $$=new absyn::NameAndTy($2,$4); }
;

ty:
    ID { $$=new absyn::NameTy(scanner_.GetTokPos(),$1); }
|   ARRAY OF ID { $$=new absyn::ArrayTy(scanner_.GetTokPos(),$3);}
|   LBRACE tyfields RBRACE { $$=new absyn::RecordTy(scanner_.GetTokPos(),$2); }
;

/*  DEFINATION CFG : record and field define */
tyfields:{$$=new absyn::FieldList();}
|   tyfields_nonempty {$$=($1);}
;

tyfields_nonempty:
    tyfield {$$=new absyn::FieldList($1);}
|   tyfield COMMA tyfields_nonempty {$$=($3)->Prepend($1);}
;

tyfield:
    ID COLON ID {$$=new absyn::Field(scanner_.GetTokPos(),$1,$3);}
;

/*  DEFINATION CFG : Erecord and Efield define */
rec:{$$=new absyn::EFieldList();}
|   rec_nonempty {$$=($1);}
;

rec_nonempty:
    rec_one {$$=new absyn::EFieldList($1);}
|   rec_one COMMA rec_nonempty {$$=($3)->Prepend($1);}
;

rec_one:
    ID EQ exp {$$=new absyn::EField($1,$3);}
;

/*  DEFINATION CFG : var define */
vardec:
    VAR ID ASSIGN exp { $$ = new absyn::VarDec(scanner_.GetTokPos(),$2,nullptr,$4);}
|   VAR ID COLON ID ASSIGN exp { $$ = new absyn::VarDec(scanner_.GetTokPos(),$2,$4,$6);}
;


/********************/
/*  EXPRESSION CFG  */
/********************/
expseq:
    sequencing { $$ = new absyn::SeqExp(scanner_.GetTokPos(),$1);}
;

sequencing:{ $$ = new absyn::ExpList();}
|   exp  { $$ = new absyn::ExpList($1);}
|   exp SEMICOLON sequencing  { $$ = ($3)->Prepend($1);}     
;

/*  EXPRESSION CFG : left value */
lvalue:  
    ID {$$ = new absyn::SimpleVar(scanner_.GetTokPos(), $1);}
|   ID LBRACK exp RBRACK {$$ = new absyn::SubscriptVar(scanner_.GetTokPos(), new absyn::SimpleVar(scanner_.GetTokPos(), $1), $3);}
|   lvalue DOT ID {$$ = new absyn::FieldVar(scanner_.GetTokPos(), $1, $3);}
|   lvalue LBRACK exp RBRACK {$$=new absyn::SubscriptVar(scanner_.GetTokPos(),$1,$3);}
;

/*  EXPRESSION CFG : arguments list */
actuals: {$$=new absyn::ExpList();}
|   nonemptyactuals {$$=($1);}
;

nonemptyactuals:
    exp {$$=new absyn::ExpList($1);}
|   exp COMMA nonemptyactuals {$$=($3)->Prepend($1);}
;

exp:
/*空值匹配*/
    NIL {$$=new absyn::NilExp(scanner_.GetTokPos());}
/*括号匹配*/
|   LPAREN RPAREN {$$=new absyn::VoidExp(scanner_.GetTokPos());}
|   LPAREN exp RPAREN {$$ = $2;}
|   LPAREN expseq RPAREN {$$ = $2;}
/*数字、字符窜匹配*/
|   INT {$$=new absyn::IntExp(scanner_.GetTokPos(),$1);}
|   STRING {$$=new absyn::StringExp(scanner_.GetTokPos(),$1);}
/*左值与赋值语句匹配*/
|   lvalue {$$ = new absyn::VarExp(scanner_.GetTokPos(),$1);}
|   lvalue ASSIGN exp {$$=new absyn::AssignExp(scanner_.GetTokPos(),$1,$3);}
/*数组中元素匹配*/
|   ID LBRACK exp RBRACK OF exp {$$=new absyn::ArrayExp(scanner_.GetTokPos(),$1,$3,$6);}
/*记录中元素匹配*/
|   ID LBRACE rec RBRACE {$$=new absyn::RecordExp(scanner_.GetTokPos(),$1,$3);}
/*调用匹配*/
|   ID LPAREN actuals RPAREN {$$=new absyn::CallExp(scanner_.GetTokPos(),$1,$3);}
/*let in作用域语句匹配*/
|   LET decs IN expseq END {$$ =new absyn::LetExp(scanner_.GetTokPos(),$2,$4);}
/*if条件分支语句匹配*/
|   IF exp THEN exp ELSE exp {$$=new absyn::IfExp(scanner_.GetTokPos(),$2,$4,$6);}
|   IF exp THEN exp {$$=new absyn::IfExp(scanner_.GetTokPos(),$2,$4,nullptr);}  
/*for while循环分支匹配*/
|   WHILE exp DO exp {$$=new absyn::WhileExp(scanner_.GetTokPos(),$2,$4);}
|   FOR ID ASSIGN exp TO exp DO exp {$$=new absyn::ForExp(scanner_.GetTokPos(),$2,$4,$6,$8);}
/*break语句匹配*/
|   BREAK {$$ = new absyn::BreakExp(scanner_.GetTokPos());}
/*负值匹配*/
|   MINUS exp {
        $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::MINUS_OP, new absyn::IntExp(scanner_.GetTokPos(), 0), $2);
    }   %prec UMINUS /*按照负值运算符优先级进行运算*/
/*二元运算语句（含逻辑和算术二元运算）匹配*/
|   exp PLUS exp {$$ = new absyn::OpExp(scanner_.GetTokPos(),absyn::Oper::PLUS_OP, $1,$3);}
|   exp MINUS exp {$$ = new absyn::OpExp(scanner_.GetTokPos(),absyn::Oper::MINUS_OP, $1,$3);}
|   exp TIMES exp {$$ = new absyn::OpExp(scanner_.GetTokPos(),absyn::Oper::TIMES_OP, $1,$3);}
|   exp DIVIDE exp {$$ = new absyn::OpExp(scanner_.GetTokPos(),absyn::Oper::DIVIDE_OP, $1,$3);}
|   exp EQ exp {$$ = new absyn::OpExp(scanner_.GetTokPos(),absyn::Oper::EQ_OP, $1,$3);}
|   exp NEQ exp {$$ = new absyn::OpExp(scanner_.GetTokPos(),absyn::Oper::NEQ_OP, $1,$3);}
|   exp LT exp {$$ = new absyn::OpExp(scanner_.GetTokPos(),absyn::Oper::LT_OP, $1,$3);}
|   exp LE exp {$$ = new absyn::OpExp(scanner_.GetTokPos(),absyn::Oper::LE_OP, $1,$3);}
|   exp GT exp {$$ = new absyn::OpExp(scanner_.GetTokPos(),absyn::Oper::GT_OP, $1,$3);}
|   exp GE exp {$$ = new absyn::OpExp(scanner_.GetTokPos(),absyn::Oper::GE_OP, $1,$3);}
|   exp AND exp {$$ = new absyn::OpExp(scanner_.GetTokPos(),absyn::Oper::AND_OP, $1,$3);}
|   exp OR exp {$$ = new absyn::OpExp(scanner_.GetTokPos(),absyn::Oper::OR_OP, $1,$3);}
;

 /* TODO: Put your lab3 code here */
