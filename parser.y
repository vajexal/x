%require "3.8"
%language "c++"

%define api.value.type variant
%define api.token.constructor
%define api.token.prefix {TOK_}
%define parse.error verbose
%define parse.assert
%define parse.trace
%expect 0
%locations

%param { Driver &driver }

%code requires {
    #include "ast.h"
    #include "driver.h"
    #include "utils.h"

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
%left '*' '/' '%'
%precedence '!'
%right POW

%token IMPORT "import"
%token INT_TYPE "int"
%token FLOAT_TYPE "float"
%token BOOL_TYPE "bool"
%token STRING_TYPE "string"
%token VOID_TYPE "void"
%token AUTO_TYPE "auto"
%token FN "fn"
%token VAR "var"
%token <std::string> IDENTIFIER
%token CASE "case"
%token DEFAULT "default"
%token IF "if"
%token ELSE "else"
%token ELSE_IF "else if"
%token WHILE "while"
%token FOR "for"
%token IN "in"
%token RANGE "range"
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
%token <double> FLOAT
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
%token THIS "this"
%token SELF "self"

%nterm <TopStatementListNode *> top_statement_list
%nterm <Node *> top_statement
%nterm <StatementListNode *> statement_list
%nterm <StatementListNode *> statement_block
%nterm <Node *> statement
%nterm <CommentNode *> maybe_comment
%nterm <ExprNode *> expr
%nterm <Type> type
%nterm <Type> array_type
%nterm <Type> return_type
%nterm <DeclNode *> var_decl
%nterm <VarNode *> identifier
%nterm <std::string> static_identifier
%nterm <ScalarNode *> scalar
%nterm <ExprNode *> dereferenceable
%nterm <std::vector<ExprNode *>> expr_list
%nterm <std::vector<ExprNode *>> non_empty_expr_list
%nterm <IfNode *> if_statement
%nterm <StatementListNode *> else_statement
%nterm <WhileNode *> while_statement
%nterm <ForNode *> for_statement
%nterm <RangeNode *> range
%nterm <FnDeclNode *> fn_decl
%nterm <FnDefNode *> fn_def
%nterm <std::vector<ArgNode *>> decl_arg_list
%nterm <std::vector<ArgNode *>> non_empty_decl_arg_list
%nterm <ArgNode *> decl_arg
%nterm <ClassNode *> class_decl
%nterm <bool> abstract_modifier
%nterm <std::string> extends
%nterm <std::vector<std::string>> implements
%nterm <StatementListNode *> class_members_list
%nterm <StatementListNode *> class_members_block
%nterm <Node *> class_member
%nterm <PropDeclNode *> prop_decl
%nterm <MethodDefNode *> method_def
%nterm <AccessModifier> access_modifier
%nterm <bool> optional_static
%nterm <std::vector<std::string>> class_name_list
%nterm <InterfaceNode *> interface_decl
%nterm <std::vector<std::string>> extends_list
%nterm <StatementListNode *> interface_methods_list
%nterm <StatementListNode *> interface_methods_block
%nterm <MethodDeclNode *> method_decl

%%

start:
top_statement_list { driver.root = $1; }
;

maybe_comment:
%empty { $$ = nullptr; }
| COMMENT { $$ = new CommentNode(std::move($1)); }
;

top_statement_list:
%empty { $$ = new TopStatementListNode; }
| top_statement_list top_statement maybe_comment '\n' {
    $1->add($2);
    if ($3) $1->add($3);
    $$ = $1;
}
| top_statement_list '\n' { $$ = $1; }
;

top_statement:
class_decl { $$ = $1; }
| interface_decl { $$ = $1; }
| fn_def { $$ = $1; }
| var_decl { $$ = $1; }
| COMMENT { $$ = new CommentNode(std::move($1)); }
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

statement_block:
'{' '}' { $$ = new StatementListNode; }
| '{' maybe_comment '\n' statement_list '}' {
    if ($2) $4->prepend($2);
    $$ = $4;
}
;

statement:
expr { $$ = $1; }
| var_decl { $$ = $1; }
| IDENTIFIER '=' expr { $$ = new AssignNode(std::move($1), $3); }
| dereferenceable '.' IDENTIFIER '=' expr { $$ = new AssignPropNode($1, std::move($3), $5); }
| static_identifier SCOPE IDENTIFIER '=' expr { $$ = new AssignStaticPropNode(std::move($1), std::move($3), $5); }
| dereferenceable '[' expr ']' '=' expr { $$ = new AssignArrNode($1, $3, $6); }
| dereferenceable '[' ']' '=' expr { $$ = new AppendArrNode($1, $5); }
| if_statement { $$ = $1; }
| while_statement { $$ = $1; }
| for_statement { $$ = $1; }
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
| expr '%' expr { $$ = new BinaryNode(OpType::MOD, $1, $3); }
| expr EQUAL expr { $$ = new BinaryNode(OpType::EQUAL, $1, $3); }
| expr NOT_EQUAL expr { $$ = new BinaryNode(OpType::NOT_EQUAL, $1, $3); }
| expr '<' expr { $$ = new BinaryNode(OpType::SMALLER, $1, $3); }
| expr SMALLER_OR_EQUAL expr { $$ = new BinaryNode(OpType::SMALLER_OR_EQUAL, $1, $3); }
| expr '>' expr { $$ = new BinaryNode(OpType::GREATER, $1, $3); }
| expr GREATER_OR_EQUAL expr { $$ = new BinaryNode(OpType::GREATER_OR_EQUAL, $1, $3); }
| scalar { $$ = std::move($1); }
| dereferenceable { $$ = std::move($1); }
| IDENTIFIER '(' expr_list ')' { $$ = new FnCallNode(std::move($1), std::move($3)); }
| static_identifier SCOPE IDENTIFIER { $$ = new FetchStaticPropNode(std::move($1), std::move($3)); }
| static_identifier SCOPE IDENTIFIER '(' expr_list ')' { $$ = new StaticMethodCallNode(std::move($1), std::move($3), std::move($5)); }
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
'[' ']' type { $$ = std::move(Type::array(std::move($3))); }
;

return_type:
type { $$ = std::move($1); }
| SELF { $$ = std::move(Type::selfTy()); }
;

var_decl:
type IDENTIFIER '=' expr { $$ = new DeclNode(std::move($1), std::move($2), $4); }
| AUTO_TYPE IDENTIFIER '=' expr { $$ = new DeclNode(Type::autoTy(), std::move($2), $4); }
| type IDENTIFIER { $$ = new DeclNode(std::move($1), std::move($2)); }
;

identifier:
IDENTIFIER { $$ = new VarNode(std::move($1)); }
;

static_identifier:
SELF { $$ = SELF_KEYWORD; }
| IDENTIFIER { $$ = std::move($1); }
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
identifier { $$ = $1; }
| THIS { $$ = new VarNode(THIS_KEYWORD); }
| '(' expr ')' { $$ = $2; }
| dereferenceable '.' IDENTIFIER { $$ = new FetchPropNode($1, std::move($3)); }
| dereferenceable '[' expr ']' { $$ = new FetchArrNode($1, $3); }
| dereferenceable '.' IDENTIFIER '(' expr_list ')' { $$ = new MethodCallNode($1, std::move($3), std::move($5)); }
| dereferenceable '.' '\n' IDENTIFIER '(' expr_list ')' { $$ = new MethodCallNode($1, std::move($4), std::move($6)); }
;

if_statement:
IF expr statement_block else_statement { $$ = new IfNode($2, $3, $4); }
;

else_statement:
%empty { $$ = nullptr; }
| ELSE statement_block { $$ = $2; }
/* todo left recursion */
| ELSE_IF expr statement_block else_statement { $$ = new StatementListNode; $$->add(new IfNode($2, $3, $4)); }
;

while_statement:
WHILE expr statement_block { $$ = new WhileNode($2, $3); }
;

for_statement:
FOR IDENTIFIER IN expr statement_block { $$ = new ForNode(std::move($2), $4, $5); }
| FOR IDENTIFIER ',' IDENTIFIER IN expr statement_block { $$ = new ForNode(std::move($2), std::move($4), $6, $7); }
| FOR IDENTIFIER IN range statement_block { $$ = new ForNode(std::move($2), $4, $5); }
;

range:
RANGE '(' expr ')' { $$ = new RangeNode($3); }
| RANGE '(' expr ',' expr ')' { $$ = new RangeNode($3, $5); }
| RANGE '(' expr ',' expr ',' expr ')' { $$ = new RangeNode($3, $5, $7); }
;

fn_def:
FN IDENTIFIER '(' decl_arg_list ')' return_type statement_block { $$ = new FnDefNode(std::move($2), std::move($4), std::move($6), $7); }
;

fn_decl:
FN IDENTIFIER '(' decl_arg_list ')' return_type { $$ = new FnDeclNode(std::move($2), std::move($4), std::move($6)); }
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
abstract_modifier CLASS IDENTIFIER extends implements class_members_block { $$ = new ClassNode(std::move($3), $6, std::move($4), std::move($5), $1); }
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

class_members_block:
'{' '}' { $$ = new StatementListNode; }
| '{' maybe_comment '\n' class_members_list '}' {
    if ($2) $4->prepend($2);
    $$ = $4;
}
;

class_members_list:
%empty { $$ = new StatementListNode; }
| class_members_list class_member maybe_comment '\n' {
    $1->add($2);
    if ($3) $1->add($3);
    $$ = $1;
}
| class_members_list '\n' { $$ = $1; }
;

class_member:
COMMENT { $$ = new CommentNode(std::move($1)); }
| prop_decl { $$ = $1; }
| method_def { $$ = $1; }
/* todo move ABSTRACT to method_decl, check here that we add only abstract method and check that interface method decl isn't abstract */
| ABSTRACT method_decl { $2->setAbstract(); $$ = $2; }
;

prop_decl:
access_modifier optional_static var_decl { $$ = new PropDeclNode($3, $1, $2); }
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
INTERFACE IDENTIFIER extends_list interface_methods_block { $$ = new InterfaceNode(std::move($2), std::move($3), $4); }
;

extends_list:
%empty { $$ = std::vector<std::string>(); }
| EXTENDS class_name_list { $$ = std::move($2); }
;

interface_methods_block:
'{' '}' { $$ = new StatementListNode; }
| '{' maybe_comment '\n' interface_methods_list '}' {
    if ($2) $4->prepend($2);
    $$ = $4;
}
;

interface_methods_list:
%empty { $$ = new StatementListNode; }
| interface_methods_list method_decl maybe_comment '\n' {
    $1->add($2);
    if ($3) $1->add($3);
    $$ = $1;
}
| interface_methods_list '\n' { $$ = $1; }
;

method_decl:
access_modifier optional_static fn_decl { $$ = new MethodDeclNode($3, $1, $2); }
;

%%

void yy::parser::error(const location_type& loc, const std::string &msg) {
    std::cerr << msg << " on line " << loc.begin.line << std::endl;
}
