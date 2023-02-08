#include "parse_code.h"

#include "driver.h"
#include "parser.tab.hh"

namespace X::Pipes {
    TopStatementListNode *ParseCode::handle(TopStatementListNode *node) {
        Driver driver(code);
        yy::parser parser(driver);

        if (parser()) {
            throw ParseCodeException("parser error");
        }

        return driver.root;
    }
}
