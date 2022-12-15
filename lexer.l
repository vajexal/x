#include <string>
#include <cstdlib>

#include "parser.tab.hh"

/*!maxnmatch:re2c*/

yy::parser::symbol_type yylex(Driver &driver) {
    const char *YYMARKER;
    const char *yypmatch[YYMAXNMATCH * 2];
    size_t yynmatch;
    driver.location.step();

    while (*driver.s == ' ' || *driver.s == '\t') {
        driver.s++;
    }

    auto start = driver.s;

    /*!stags:re2c format = 'const char *@@; '; */

    /*!re2c
      re2c:define:YYCTYPE = char;
      re2c:define:YYCURSOR = driver.s;
      re2c:yyfill:enable = 0;
      re2c:posix-captures = 1;

      "import" { return yy::parser::make_IMPORT(driver.location); }
      "int" { return yy::parser::make_INT_TYPE(driver.location); }
      "float" { return yy::parser::make_FLOAT_TYPE(driver.location); }
      "bool" { return yy::parser::make_BOOL_TYPE(driver.location); }
      "string" { return yy::parser::make_STRING_TYPE(driver.location); }
      "void" { return yy::parser::make_VOID_TYPE(driver.location); }
      "false" { return yy::parser::make_BOOL(false, driver.location); }
      "true" { return yy::parser::make_BOOL(true, driver.location); }
      "fn" { return yy::parser::make_FN(driver.location); }
      "var" { return yy::parser::make_VAR(driver.location); }
      "case" { return yy::parser::make_CASE(driver.location); }
      "default" { return yy::parser::make_DEFAULT(driver.location); }
      "if" { return yy::parser::make_IF(driver.location); }
      "else" { return yy::parser::make_ELSE(driver.location); }
      "else if" { return yy::parser::make_ELSE_IF(driver.location); }
      "while" { return yy::parser::make_WHILE(driver.location); }
      "for" { return yy::parser::make_FOR(driver.location); }
      "in" { return yy::parser::make_IN(driver.location); }
      "range" { return yy::parser::make_RANGE(driver.location); }
      "continue" { return yy::parser::make_CONTINUE(driver.location); }
      "break" { return yy::parser::make_BREAK(driver.location); }
      "return" { return yy::parser::make_RETURN(driver.location); }
      "println" { return yy::parser::make_PRINTLN(driver.location); }
      "class" { return yy::parser::make_CLASS(driver.location); }
      "abstract" { return yy::parser::make_ABSTRACT(driver.location); }
      "interface" { return yy::parser::make_INTERFACE(driver.location); }
      "implements" { return yy::parser::make_IMPLEMENTS(driver.location); }
      "extends" { return yy::parser::make_EXTENDS(driver.location); }
      "new" { return yy::parser::make_NEW(driver.location); }
      "static" { return yy::parser::make_STATIC(driver.location); }
      "::" { return yy::parser::make_SCOPE(driver.location); }
      "public" { return yy::parser::make_PUBLIC(driver.location); }
      "protected" { return yy::parser::make_PROTECTED(driver.location); }
      "private" { return yy::parser::make_PRIVATE(driver.location); }

      [_a-zA-Z][_a-zA-Z0-9]* { return yy::parser::make_IDENTIFIER(std::string(yypmatch[0], yypmatch[1] - yypmatch[0]), driver.location); }

      "++" { return yy::parser::make_INC(driver.location); }
      "--" { return yy::parser::make_DEC(driver.location); }
      "**" { return yy::parser::make_POW(driver.location); }
      "||" { return yy::parser::make_OR(driver.location); }
      "&&" { return yy::parser::make_AND(driver.location); }
      "==" { return yy::parser::make_EQUAL(driver.location); }
      "!=" { return yy::parser::make_NOT_EQUAL(driver.location); }
      "<=" { return yy::parser::make_SMALLER_OR_EQUAL(driver.location); }
      ">=" { return yy::parser::make_GREATER_OR_EQUAL(driver.location); }

      "//"([^\n\x00]*) { return yy::parser::make_COMMENT(std::string(yypmatch[2], yypmatch[3] - yypmatch[2]), driver.location); }

      [+\-]?[0-9]+"."[0-9]+ { return yy::parser::make_FLOAT(std::atof(yypmatch[0]), driver.location); }
      "0"|[+\-]?[1-9][0-9]* { return yy::parser::make_INT(std::atol(yypmatch[0]), driver.location); }
      ["]([^"\n\x00]*)["] { return yy::parser::make_STRING(std::string(yypmatch[2], yypmatch[3] - yypmatch[2]), driver.location); }

     "\n" { driver.location.lines(); return yy::parser::symbol_type(*start, driver.location); }
      * { return yy::parser::symbol_type(*start, driver.location); }
     */
}
