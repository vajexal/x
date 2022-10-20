#include "parse_code.h"

#include "driver.h"
#include "parser.tab.hh"

namespace X::Pipes {
    StatementListNode *ParseCode::handle(StatementListNode *node) {
        Driver driver(code);
        yy::parser parser(driver);

        if (parser()) {
            throw ParseCodeException("parser error");
        }

        return driver.root;
    }
}
