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

%token
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK
  LBRACE RBRACE DOT
  ASSIGN
  ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF
  BREAK NIL
  FUNCTION VAR TYPE

 /* token priority */
%left OR
%left AND
%nonassoc EQ NEQ LT LE GT GE
%left PLUS MINUS
%left TIMES DIVIDE
%left UMINUS
 /* TODO: Put your lab3 code here */

%type <exp> exp expSequence operatorExp ifExp whileExp callExp recordExp
%type <explist> sequencing sequencingExps
%type <dec> decsNonempty_s varDec
%type <declist> decs decsNonempty
%type <efield> recOne
%type <efieldlist> rec
%type <fieldlist> typeFields
%type <ty> ty
%type <tydec> tydecOne
%type <tydeclist> tydec
%type <fundec> fundecOne
%type <fundeclist> fundec
%type <var> lvalue

%start program

%% 
program:  exp  {absyn_tree_ = std::make_unique<absyn::AbsynTree>($1);}; // init our abstract tree 

operatorExp:
  exp EQ exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::EQ_OP, $1, $3);} |
  exp NEQ exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::NEQ_OP, $1, $3);} |
  exp LT exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::LT_OP, $1, $3);} |
  exp LE exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::LE_OP, $1, $3);} |
  exp GT exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::GT_OP, $1, $3);} |
  exp GE exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::GE_OP, $1, $3);} |
  exp PLUS exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::PLUS_OP, $1, $3);} |
  exp MINUS exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::MINUS_OP, $1, $3);} |
  exp TIMES exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::TIMES_OP, $1, $3);} |
  exp DIVIDE exp  {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::DIVIDE_OP, $1, $3);} |
  exp AND exp { $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::AND_OP, $1, $3); }|
  exp OR exp  { $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::OR_OP, $1, $3); }|
  MINUS exp %prec UMINUS {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::MINUS_OP, new absyn::IntExp(scanner_.GetTokPos(), 0), $2);}; // -a = 0-a, %prec UMINUS sets the precedence level of the unary minus operation

ifExp:
  IF exp THEN exp {$$ = new absyn::IfExp(scanner_.GetTokPos(), $2, $4, nullptr);} |                                 // no else 
  IF LPAREN exp RPAREN THEN exp {$$ = new absyn::IfExp(scanner_.GetTokPos(), $3, $6, nullptr);} |                   // with () no else
  IF exp THEN exp ELSE exp {$$ = new absyn::IfExp(scanner_.GetTokPos(), $2, $4, $6);} |                             // with else 
  IF LPAREN exp RPAREN THEN exp ELSE exp {$$ = new absyn::IfExp(scanner_.GetTokPos(), $3, $6, $8);} |               // with () and else
  exp AND exp {$$ = new absyn::IfExp(scanner_.GetTokPos(), $1, $3, new absyn::IntExp(scanner_.GetTokPos(), 0));} |  // check a then b else 0 
  exp OR exp {$$ = new absyn::IfExp(scanner_.GetTokPos(), $1, new absyn::IntExp(scanner_.GetTokPos(), 1), $3);};    // check a then 1 else check b

whileExp:
  WHILE exp DO exp {$$ = new absyn::WhileExp(scanner_.GetTokPos(), $2, $4);} |                                      // without ()
  WHILE LPAREN exp RPAREN DO exp {$$ = new absyn::WhileExp(scanner_.GetTokPos(), $3, $6);};                         // with ()
  // exp AND exp {$$ = new absyn::WhileExp(scanner_.GetTokPos(), $1, $3, new absyn::IntExp(scanner_.GetTokPos(), 0));} |  // check a then b else 0 
  // exp OR exp {$$ = new absyn::WhileExp(scanner_.GetTokPos(), $1, new absyn::IntExp(scanner_.GetTokPos(), 1), $3);};    // check a then 1 else check b

callExp:
  ID LPAREN RPAREN {$$ = new absyn::CallExp(scanner_.GetTokPos(), $1, new absyn::ExpList());} |                     // no param
  ID LPAREN sequencingExps RPAREN {$$ = new absyn::CallExp(scanner_.GetTokPos(), $1, $3);};                        // with param


recordExp:
  ID LBRACE RBRACE {$$ = new absyn::RecordExp(scanner_.GetTokPos(), $1, new absyn::EFieldList());} |                // without fields: Point{}
  ID LBRACE rec RBRACE {$$ = new absyn::RecordExp(scanner_.GetTokPos(), $1, $3);};                                  // with fields  : Point{x=1, y=0}

exp:
  INT {$$ = new absyn::IntExp(scanner_.GetTokPos(), $1);} |
  STRING {$$ = new absyn::StringExp(scanner_.GetTokPos(), $1);} |
  NIL {$$ = new absyn::NilExp(scanner_.GetTokPos());} |                                                                 
  BREAK {$$ = new absyn::BreakExp(scanner_.GetTokPos());} |                                                        
  lvalue {$$ = new absyn::VarExp(scanner_.GetTokPos(), $1);} |                                                      // x
  lvalue ASSIGN exp {$$ = new absyn::AssignExp(scanner_.GetTokPos(), $1, $3);} |                                    // x:= E
  LET decs IN expSequence END {$$ = new absyn::LetExp(scanner_.GetTokPos(), $2, $4);} |                             // let strucuture
  ID LBRACK exp RBRACK OF exp {$$ = new absyn::ArrayExp(scanner_.GetTokPos(), $1, $3, $6);} |                       // var row := intArray [ N ] of 0 (queens)
  LPAREN exp RPAREN {$$ = $2; } |                                                                                   // (x)
  LPAREN RPAREN {$$ = new absyn::VoidExp(scanner_.GetTokPos());} |                                                  // ()
  LPAREN sequencingExps RPAREN {$$ = new absyn::SeqExp(scanner_.GetTokPos(), $2);} |                                // (x := 5; y := 6)
  FOR ID ASSIGN exp TO exp DO exp {$$ = new absyn::ForExp(scanner_.GetTokPos(), $2, $4, $6, $8);} |                 // for i := 1 to 10 do x := x + i
  operatorExp |  ifExp | whileExp | callExp | recordExp;
 
recOne:
  ID EQ exp {$$ = new absyn::EField($1, $3);};                                                                      // x = 1

rec:
  recOne {$$ = new absyn::EFieldList($1);} |                                                                       // x = 1
  recOne COMMA rec {$$ = $3; $$->Prepend($1);};                                                                    // x = 1, y=1

expSequence:
  sequencingExps {$$ = new absyn::SeqExp(scanner_.GetTokPos(), $1);};                                              //

sequencingExps:
  exp {$$ = new absyn::ExpList($1);} |
  exp COMMA sequencingExps {$$ = $3; $$->Prepend($1);} |                                                          // x := 1, y := 2, push y:=2 to exp_list_
  exp SEMICOLON sequencingExps {$$ = $3; $$->Prepend($1); };                                                      // x := 1; y := 2


varDec:
  VAR ID ASSIGN exp {$$ = new absyn::VarDec(scanner_.GetTokPos(), $2, nullptr, $4);} |                             // var x := 10
  VAR ID COLON ID ASSIGN exp {$$ = new absyn::VarDec(scanner_.GetTokPos(), $2, $4, $6);};                          // var x: int := 10


typeFields:
  ID COLON ID {$$ = new absyn::FieldList(new absyn::Field(scanner_.GetTokPos(), $1, $3));} |                       //  x: int
  ID COLON ID COMMA typeFields {$$ = $5; $$->Prepend(new absyn::Field(scanner_.GetTokPos(), $1, $3));};            //  x: int, y: int  push y:int to field_list_

ty:
  ARRAY OF ID {$$ = new absyn::ArrayTy(scanner_.GetTokPos(), $3);} |                                               // array of int
  ID {$$ = new absyn::NameTy(scanner_.GetTokPos(), $1);} |                                                         // int
  LBRACE typeFields RBRACE {$$ = new absyn::RecordTy(scanner_.GetTokPos(), $2);};                                   // {x: int, y: int}

tydecOne:
  ID EQ ty {$$ = new absyn::NameAndTy($1, $3);};                                                                  // intList = array of int

tydec:
  TYPE tydecOne {$$ = new absyn::NameAndTyList($2);} |                                                           // type intList = array of int;
  TYPE tydecOne tydec {$$ = $3; $$->Prepend($2);};                                                               // type intList = array of int; type point = {x: int, y: int}
        

// FunDec(int pos, sym::Symbol *name, FieldList *params, sym::Symbol *result, Exp *body)
fundecOne:

  ID LPAREN RPAREN EQ exp {$$ = new absyn::FunDec(scanner_.GetTokPos(), $1, new absyn::FieldList(), nullptr, $5);}    |                 // foo() = 1
  ID LPAREN RPAREN EQ LPAREN exp RPAREN {$$ = new absyn::FunDec(scanner_.GetTokPos(), $1, new absyn::FieldList(), nullptr, $6);}  |     // foo() = (1)

  ID LPAREN RPAREN COLON ID EQ exp {$$ = new absyn::FunDec(scanner_.GetTokPos(), $1, new absyn::FieldList(), $5, $7);}          |       // foo(): int = 1
  ID LPAREN RPAREN COLON ID EQ LPAREN exp RPAREN {$$ = new absyn::FunDec(scanner_.GetTokPos(), $1, new absyn::FieldList(), $5, $8);}  | // foo(): int = (1)

  ID LPAREN typeFields RPAREN EQ exp {$$ = new absyn::FunDec(scanner_.GetTokPos(), $1, $3, nullptr, $6);}                 |               // foo(a: int, b: int) = a + b
  ID LPAREN typeFields RPAREN EQ LPAREN exp RPAREN {$$ = new absyn::FunDec(scanner_.GetTokPos(), $1, $3, nullptr, $7);}       |           // foo(a: int, b: int) = (a + b)

  ID LPAREN typeFields RPAREN COLON ID EQ exp {$$ = new absyn::FunDec(scanner_.GetTokPos(), $1, $3, $6, $8);}                |            // foo(a: int, b: int): int = a + b
  ID LPAREN typeFields RPAREN COLON ID EQ LPAREN exp RPAREN {$$ = new absyn::FunDec(scanner_.GetTokPos(), $1, $3, $6, $9);};             // foo(a: int, b: int): int = (a + b)


fundec:
  FUNCTION fundecOne {$$ = new absyn::FunDecList($2);} |                                                                           // function foo(a: int): int = a + 1
  FUNCTION fundecOne fundec {$$ = $3; $$->Prepend($2);};                                                                           // function foo(a: int): int = a + 1; function bar(b: int): int = b + 2


decsNonempty_s:
  tydec {$$ = new absyn::TypeDec(scanner_.GetTokPos(), $1);} |                                                                     // type intList = array of int           
  fundec {$$ = new absyn::FunctionDec(scanner_.GetTokPos(), $1);};                                                                 // function foo(a: int): int = a + 1

decs:
  decsNonempty_s {$$ = new absyn::DecList($1);} |                                                                                // type intList = array of int
  decsNonempty_s decs {$$ = $2; $$->Prepend($1);} |                                                                              // type intList = array of int; var x: int := 1                         
  varDec {$$ = new absyn::DecList($1);} |                                                                                         // var x: int := 1
  varDec decs {$$ = $2; $$->Prepend($1);};                                                                                        // var x: int := 1; var y: int := 2

lvalue:
  lvalue DOT ID {$$ = new absyn::FieldVar(scanner_.GetTokPos(), $1, $3);} |                                                       // person.name
  lvalue LBRACK exp RBRACK {$$ = new absyn::SubscriptVar(scanner_.GetTokPos(), $1, $3);} |                                        // array1[array2[1]]
  ID {$$ = new absyn::SimpleVar(scanner_.GetTokPos(), $1);}|                                                                      // variable
  ID LBRACK exp RBRACK {$$ = new absyn::SubscriptVar(scanner_.GetTokPos(), new absyn::SimpleVar(scanner_.GetTokPos(), $1), $3);}; // array[2]
