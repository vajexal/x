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

%left OR
%left AND
%nonassoc EQUAL NOT_EQUAL
%nonassoc '<' SMALLER_OR_EQUAL '>' GREATER_OR_EQUAL
%left '+' '-'
%left '*' '/'
%precedence '!'
%right POW

%token IMPORT "import"
%token INT_TYPE "int"
%token FLOAT_TYPE "float"
%token BOOL_TYPE "bool"
%token STRING_TYPE "string"
%token VOID_TYPE "void"
%token FN "fn"
%token VAR "var"
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
%token PRINTLN "println"
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
%token CLASS "class"
%token INTERFACE "interface"
%token IMPLEMENTS "implements"
%token EXTENDS "extends"
%token NEW "new"
%token STATIC "static"
%token SCOPE "::"
%token PUBLIC "public"
%token PROTECTED "protected"
%token PRIVATE "private"
%token ABSTRACT "abstract"

%nterm <StatementListNode *> top_statement_list
%nterm <Node *> top_statement
%nterm <StatementListNode *> statement_list
%nterm <Node *> statement
%nterm <CommentNode *> maybe_comment
%nterm <ExprNode *> expr
%nterm <Type> type
%nterm <Type> array_type
%nterm <DeclareNode *> var_decl
%nterm <VarNode *> identifier
%nterm <ScalarNode *> scalar
%nterm <ExprNode *> dereferenceable
%nterm <std::vector<ExprNode *>> expr_list
%nterm <std::vector<ExprNode *>> non_empty_expr_list
%nterm <IfNode *> if_statement
%nterm <WhileNode *> while_statement
%nterm <FnDeclNode *> fn_decl
%nterm <FnDefNode *> fn_def
%nterm <std::vector<ArgNode *>> decl_arg_list
%nterm <std::vector<ArgNode *>> non_empty_decl_arg_list
%nterm <ArgNode *> decl_arg
%nterm <ClassNode *> class_decl
%nterm <bool> abstract_modifier
%nterm <std::string> extends
%nterm <std::vector<std::string>> implements
%nterm <ClassMembersNode *> class_members_list
%nterm <PropDeclNode *> prop_decl
%nterm <MethodDefNode *> method_def
%nterm <AccessModifier> access_modifier
%nterm <bool> optional_static
%nterm <std::vector<std::string>> class_name_list
%nterm <InterfaceNode *> interface_decl
%nterm <std::vector<std::string>> extends_list
%nterm <std::vector<MethodDeclNode *>> interface_methods_list
%nterm <MethodDeclNode *> method_decl

%%

start:
top_statement_list { driver.root = $1; }
;

newlines:
'\n'
| newlines '\n'
;

maybe_comment:
%empty { $$ = nullptr; }
| COMMENT { $$ = new CommentNode(std::move($1)); }
;

top_statement_list:
%empty { $$ = new StatementListNode; }
| top_statement_list top_statement maybe_comment '\n' {
    $1->add($2);
    if ($3) $1->add($3);
    $$ = $1;
}
| top_statement_list '\n' { $$ = $1; }
;

statement_list:
%empty { $$ = new StatementListNode; }
| statement_list statement maybe_comment '\n' {
    $1->add($2);
    if ($3) $1->add($3);
    $$ = $1;
}
| statement_list '\n' { $$ = $1; }
;

top_statement:
class_decl { $$ = $1; }
| interface_decl { $$ = $1; }
| fn_def { $$ = $1; }
| COMMENT { $$ = new CommentNode(std::move($1)); }
;

statement:
expr { $$ = $1; }
| var_decl { $$ = $1; }
| IDENTIFIER '=' expr { $$ = new AssignNode(std::move($1), $3); }
| dereferenceable '.' IDENTIFIER '=' expr { $$ = new AssignPropNode($1, std::move($3), $5); }
| IDENTIFIER SCOPE IDENTIFIER '=' expr { $$ = new AssignStaticPropNode(std::move($1), std::move($3), $5); }
| dereferenceable '[' expr ']' '=' expr { $$ = new AssignArrNode($1, $3, $6); }
| dereferenceable '[' ']' '=' expr { $$ = new AppendArrNode($1, $5); }
| if_statement { $$ = $1; }
| while_statement { $$ = $1; }
| BREAK { $$ = new BreakNode; }
| CONTINUE { $$ = new ContinueNode; }
| RETURN { $$ = new ReturnNode; }
| RETURN expr { $$ = new ReturnNode($2); }
| PRINTLN '(' expr ')' { $$ = new PrintlnNode($3); }
| COMMENT { $$ = new CommentNode(std::move($1)); }
;

expr:
INC identifier { $$ = new UnaryNode(OpType::PRE_INC, $2); }
| DEC identifier { $$ = new UnaryNode(OpType::PRE_DEC, $2); }
| identifier INC { $$ = new UnaryNode(OpType::POST_INC, $1); }
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
| scalar { $$ = std::move($1); }
| dereferenceable { $$ = std::move($1); }
| IDENTIFIER '(' expr_list ')' { $$ = new FnCallNode(std::move($1), std::move($3)); }
| IDENTIFIER SCOPE IDENTIFIER { $$ = new FetchStaticPropNode(std::move($1), std::move($3)); }
| IDENTIFIER SCOPE IDENTIFIER '(' expr_list ')' { $$ = new StaticMethodCallNode(std::move($1), std::move($3), std::move($5)); }
| NEW IDENTIFIER '(' expr_list ')' { $$ = new NewNode(std::move($2), std::move($4)); }
;

type:
INT_TYPE { $$ = std::move(Type::scalar(Type::TypeID::INT)); }
| FLOAT_TYPE { $$ = std::move(Type::scalar(Type::TypeID::FLOAT)); }
| BOOL_TYPE { $$ = std::move(Type::scalar(Type::TypeID::BOOL)); }
| STRING_TYPE { $$ = std::move(Type::scalar(Type::TypeID::STRING)); }
| VOID_TYPE { $$ = std::move(Type::scalar(Type::TypeID::VOID)); }
| IDENTIFIER { $$ = std::move(Type::klass(std::move($1))); }
| array_type { $$ = std::move($1); }
;

array_type:
'[' ']' type { $$ = std::move(Type::array(new Type($3))); }
;

var_decl:
type IDENTIFIER '=' expr { $$ = new DeclareNode(std::move($1), std::move($2), $4); }
| type IDENTIFIER { $$ = new DeclareNode(std::move($1), std::move($2)); }
;

identifier:
IDENTIFIER { $$ = new VarNode(std::move($1)); }
;

scalar:
INT { $$ = new ScalarNode(std::move(Type::scalar(Type::TypeID::INT)), $1); }
| FLOAT { $$ = new ScalarNode(std::move(Type::scalar(Type::TypeID::FLOAT)), $1); }
| BOOL { $$ = new ScalarNode(std::move(Type::scalar(Type::TypeID::BOOL)), $1); }
| STRING { $$ = new ScalarNode(std::move(Type::scalar(Type::TypeID::STRING)), std::move($1)); }
| array_type '{' expr_list '}' { $$ = new ScalarNode(std::move($1), std::move($3)); }
;

expr_list:
%empty { $$ = std::vector<ExprNode *>(); }
| non_empty_expr_list { $$ = std::move($1); }
;

non_empty_expr_list:
expr { $$ = std::vector<ExprNode *>(); $$.push_back($1); }
| non_empty_expr_list ',' expr { $1.push_back($3); $$ = std::move($1); }
;

dereferenceable:
identifier { $$ = std::move($1); }
| dereferenceable '.' IDENTIFIER { $$ = new FetchPropNode($1, std::move($3)); }
| dereferenceable '[' expr ']' { $$ = new FetchArrNode($1, $3); }
| dereferenceable '.' IDENTIFIER '(' expr_list ')' { $$ = new MethodCallNode($1, std::move($3), std::move($5)); }
;

if_statement:
IF expr '{' statement_list '}' { $$ = new IfNode($2, $4); }
| IF expr '{' statement_list '}' ELSE '{' statement_list '}' { $$ = new IfNode($2, $4, $8); }
;

while_statement:
WHILE expr '{' statement_list '}' { $$ = new WhileNode($2, $4); }
;

fn_def:
FN IDENTIFIER '(' decl_arg_list ')' type '{' statement_list '}' { $$ = new FnDefNode(std::move($2), std::move($4), std::move($6), $8); }
;

fn_decl:
FN IDENTIFIER '(' decl_arg_list ')' type { $$ = new FnDeclNode(std::move($2), std::move($4), std::move($6)); }
;

decl_arg_list:
%empty { $$ = std::vector<ArgNode *>(); }
| non_empty_decl_arg_list { $$ = std::move($1); }
;

non_empty_decl_arg_list:
decl_arg { $$ = std::vector<ArgNode *>(); $$.push_back($1); }
| non_empty_decl_arg_list ',' decl_arg { $1.push_back($3); $$ = std::move($1); }
;

decl_arg:
type IDENTIFIER { $$ = new ArgNode(std::move($1), std::move($2)); }
;

class_decl:
abstract_modifier CLASS IDENTIFIER extends implements '{' newlines class_members_list '}' { $$ = new ClassNode(std::move($3), $8, std::move($4), std::move($5), $1); }
;

abstract_modifier:
%empty { $$ = false; }
| ABSTRACT { $$ = true; }
;

extends:
%empty { $$ = std::string(); }
| EXTENDS IDENTIFIER { $$ = $2; }
;

implements:
%empty { $$ = std::vector<std::string>(); }
| IMPLEMENTS class_name_list { $$ = $2; }
;

class_members_list:
%empty { $$ = new ClassMembersNode; }
| class_members_list prop_decl newlines { $1->addProp($2); $$ = $1; }
| class_members_list method_def newlines { $1->addMethod($2); $$ = $1; }
| class_members_list ABSTRACT method_decl newlines { $1->addAbstractMethod($3); $$ = $1; }
;

prop_decl:
access_modifier optional_static type IDENTIFIER { $$ = new PropDeclNode(std::move($3), std::move($4), $1, $2); }
;

method_def:
access_modifier optional_static fn_def { $$ = new MethodDefNode($3, $1, $2); }
;

access_modifier:
%empty { $$ = AccessModifier::PUBLIC; }
| PUBLIC { $$ = AccessModifier::PUBLIC; }
| PROTECTED { $$ = AccessModifier::PROTECTED; }
| PRIVATE { $$ = AccessModifier::PRIVATE; }
;

optional_static:
%empty { $$ = false; }
| STATIC { $$ = true; }
;

class_name_list:
IDENTIFIER { $$ = std::vector<std::string>(); $$.push_back(std::move($1)); }
| class_name_list ',' IDENTIFIER { $$ = $1; $$.push_back(std::move($3)); }
;

interface_decl:
INTERFACE IDENTIFIER extends_list '{' newlines interface_methods_list '}' { $$ = new InterfaceNode(std::move($2), std::move($3), std::move($6)); }
;

extends_list:
%empty { $$ = std::vector<std::string>(); }
| EXTENDS class_name_list { $$ = std::move($2); }
;

interface_methods_list:
method_decl newlines { $$ = std::vector<MethodDeclNode *>(); $$.push_back($1); }
| interface_methods_list method_decl newlines { $$ = $1; $$.push_back($2); }
;

method_decl:
access_modifier optional_static fn_decl { $$ = new MethodDeclNode($3, $1, $2); }
;

%%

void yy::parser::error(const std::string &msg) {
    std::cerr << "yyerror: " << msg << std::endl;
}
