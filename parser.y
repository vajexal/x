%require "3.8"
%language "c++"

%define api.value.type variant
%define api.token.constructor
%define api.token.prefix {TOK_}
%define parse.error verbose
%define parse.assert
%define parse.trace
%expect 0

%param { Driver &driver }

%code requires {
    #include "ast.h"
    #include "driver.h"

    using namespace X;
}

%code {
    yy::parser::symbol_type yylex(Driver &driver);
}

%precedence '='
%left OR
%left AND
%nonassoc EQUAL NOT_EQUAL
%nonassoc '<' SMALLER_OR_EQUAL '>' GREATER_OR_EQUAL
%left '+' '-'
%left '*' '/'
%precedence '!'
%precedence INC DEC
%right POW
%precedence ELSE

%token IMPORT "import"
%token INT_TYPE "int"
%token FLOAT_TYPE "float"
%token BOOL_TYPE "bool"
%token VOID_TYPE "void"
%token FN "fn"
%token <std::string> IDENTIFIER
%token CASE "case"
%token DEFAULT "default"
%token IF "if"
%token ELSE "else"
%token WHILE "while"
%token IN "in"
%token CONTINUE "continue"
%token BREAK "break"
%token RETURN "return"
%token INC "++"
%token DEC "--"
%token POW "**"
%token OR "||"
%token AND "&&"
%token EQUAL "=="
%token NOT_EQUAL "!="
%token SMALLER_OR_EQUAL "<="
%token GREATER_OR_EQUAL ">="
%token <std::string> COMMENT
%token <int> INT
%token <float> FLOAT
%token <bool> BOOL
%token <std::string> STRING

%nterm <StatementListNode *> statement_list
%nterm <Node *> statement
%nterm <ExprNode *> expr
%nterm <Type> type
%nterm <VarNode *> identifier
%nterm <ScalarNode *> scalar
%nterm <std::vector<ExprNode *> *> call_arg_list
%nterm <std::vector<ExprNode *> *> non_empty_call_arg_list
%nterm <IfNode *> if_statement
%nterm <WhileNode *> while_statement
%nterm <FnNode *> fn_decl
%nterm <std::vector<ArgNode *> *> decl_arg_list
%nterm <std::vector<ArgNode *> *> non_empty_decl_arg_list
%nterm <ArgNode *> decl_arg

%%

start:
	statement_list { driver.result = $1; }
;

statement_list:
%empty { $$ = new StatementListNode; }
| statement_list statement { $1->add($2); $$ = $1; }
;

statement:
expr { $$ = $1; }
| type IDENTIFIER '=' expr { $$ = new DeclareNode($1, $2, $4); }
| IDENTIFIER '=' expr { $$ = new AssignNode($1, $3); }
| if_statement { $$ = $1; }
| while_statement { $$ = $1; }
| fn_decl { $$ = $1; }
| BREAK { $$ = new BreakNode; }
| CONTINUE { $$ = new ContinueNode; }
| RETURN expr { $$ = new ReturnNode($2); }
| COMMENT { $$ = new CommentNode($1); }
;

expr:
identifier INC { $$ = new UnaryNode(OpType::POST_INC, $1); }
| identifier DEC { $$ = new UnaryNode(OpType::POST_DEC, $1); }
| expr OR expr { $$ = new BinaryNode(OpType::OR, $1, $3); }
| expr AND expr { $$ = new BinaryNode(OpType::AND, $1, $3); }
| expr '+' expr { $$ = new BinaryNode(OpType::PLUS, $1, $3); }
| expr '-' expr { $$ = new BinaryNode(OpType::MINUS, $1, $3); }
| expr '*' expr { $$ = new BinaryNode(OpType::MUL, $1, $3); }
| expr '/' expr { $$ = new BinaryNode(OpType::DIV, $1, $3); }
| '!' expr { $$ = new UnaryNode(OpType::NOT, $2); }
| expr POW expr { $$ = new BinaryNode(OpType::POW, $1, $3); }
| expr EQUAL expr { $$ = new BinaryNode(OpType::EQUAL, $1, $3); }
| expr NOT_EQUAL expr { $$ = new BinaryNode(OpType::NOT_EQUAL, $1, $3); }
| expr '<' expr { $$ = new BinaryNode(OpType::SMALLER, $1, $3); }
| expr SMALLER_OR_EQUAL expr { $$ = new BinaryNode(OpType::SMALLER_OR_EQUAL, $1, $3); }
| expr '>' expr { $$ = new BinaryNode(OpType::GREATER, $1, $3); }
| expr GREATER_OR_EQUAL expr { $$ = new BinaryNode(OpType::GREATER_OR_EQUAL, $1, $3); }
| scalar { $$ = $1; }
| identifier { $$ = $1; }
| IDENTIFIER '(' call_arg_list ')' { $$ = new FnCallNode($1, *$3); }
;

type:
INT_TYPE { $$ = Type::INT; }
| FLOAT_TYPE { $$ = Type::FLOAT; }
| BOOL_TYPE { $$ = Type::BOOL; }
| VOID_TYPE { $$ = Type::VOID; }
;

identifier:
IDENTIFIER { $$ = new VarNode($1); }
;

scalar:
INT { $$ = new ScalarNode(Type::INT, $1); }
| FLOAT { $$ = new ScalarNode(Type::FLOAT, $1); }
| BOOL { $$ = new ScalarNode(Type::BOOL, $1); }
;

call_arg_list:
%empty { $$ = new std::vector<ExprNode *>; }
| non_empty_call_arg_list { $$ = $1; }
;

non_empty_call_arg_list:
expr { $$ = new std::vector<ExprNode *>; $$->push_back($1); }
| non_empty_call_arg_list ',' expr { $1->push_back($3); $$ = $1; }
;

if_statement:
IF expr '{' statement_list '}' { $$ = new IfNode($2, $4); }
| IF expr '{' statement_list '}' ELSE '{' statement_list '}' { $$ = new IfNode($2, $4, $8); }
;

while_statement:
WHILE expr '{' statement_list '}' { $$ = new WhileNode($2, $4); }
;

fn_decl:
FN IDENTIFIER '(' decl_arg_list ')' type '{' statement_list '}' { $$ = new FnNode($2, *$4, $6, $8); }
;

decl_arg_list:
%empty { $$ = new std::vector<ArgNode *>; }
| non_empty_decl_arg_list { $$ = $1; }
;

non_empty_decl_arg_list:
decl_arg { $$ = new std::vector<ArgNode *>; $$->push_back($1); }
| non_empty_decl_arg_list ',' decl_arg { $1->push_back($3); $$ = $1; }
;

decl_arg:
type IDENTIFIER { $$ = new ArgNode($1, $2); }
;

%%

void yy::parser::error(const std::string &msg) {
    std::cerr << "yyerror: " << msg << std::endl;
}
