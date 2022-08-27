#include <string>
#include <cstdlib>

#include "parser.tab.hh"

/*!maxnmatch:re2c*/

yy::parser::symbol_type yylex(Driver &driver) {
    const char *YYMARKER;
    const char *yypmatch[YYMAXNMATCH * 2];
    size_t yynmatch;

    while (*driver.s == ' ' || *driver.s == '\t' || *driver.s == '\n') {
        driver.s++;
    }

    const char *start = driver.s;

    /*!stags:re2c format = 'const char *@@; '; */

    /*!re2c
      re2c:define:YYCTYPE = char;
      re2c:define:YYCURSOR = driver.s;
      re2c:yyfill:enable = 0;
      re2c:posix-captures = 1;

      "import" { return yy::parser::make_IMPORT(); }
      "int" { return yy::parser::make_INT_TYPE(); }
      "float" { return yy::parser::make_FLOAT_TYPE(); }
      "bool" { return yy::parser::make_BOOL_TYPE(); }
      "void" { return yy::parser::make_VOID_TYPE(); }
      "fn" { return yy::parser::make_FN(); }
      "var" { return yy::parser::make_VAR(); }
      "case" { return yy::parser::make_CASE(); }
      "default" { return yy::parser::make_DEFAULT(); }
      "if" { return yy::parser::make_IF(); }
      "else" { return yy::parser::make_ELSE(); }
      "while" { return yy::parser::make_WHILE(); }
      "continue" { return yy::parser::make_CONTINUE(); }
      "break" { return yy::parser::make_BREAK(); }
      "return" { return yy::parser::make_RETURN(); }
      "class" { return yy::parser::make_CLASS(); }
      "new" { return yy::parser::make_NEW(); }
      "static" { return yy::parser::make_STATIC(); }
      "::" { return yy::parser::make_SCOPE(); }

      [_a-zA-Z][_a-zA-Z0-9]* { return yy::parser::make_IDENTIFIER(std::string(yypmatch[0], yypmatch[1] - yypmatch[0])); }

      "++" { return yy::parser::make_INC(); }
      "--" { return yy::parser::make_DEC(); }
      "**" { return yy::parser::make_POW(); }
      "||" { return yy::parser::make_OR(); }
      "&&" { return yy::parser::make_AND(); }
      "==" { return yy::parser::make_EQUAL(); }
      "!=" { return yy::parser::make_NOT_EQUAL(); }
      "<=" { return yy::parser::make_SMALLER_OR_EQUAL(); }
      ">=" { return yy::parser::make_GREATER_OR_EQUAL(); }

      "//"[^\n\x00]* { return yy::parser::make_COMMENT(std::string(yypmatch[0], yypmatch[1] - yypmatch[0])); }

      [0-9]+"."[0-9]+ { return yy::parser::make_FLOAT(std::atof(yypmatch[0])); }
      "0"|[1-9][0-9]* { return yy::parser::make_INT(std::atoi(yypmatch[0])); }
      ["]([^"\n\x00])*["] { return yy::parser::make_STRING(std::string(yypmatch[0], yypmatch[1] - yypmatch[0])); }

      * { return yy::parser::symbol_type(*start); }
     */
}
