#include "Print.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int Print::visit_stmt(VarType &result) {
    value->visit_expr(result);
    return 0;
}

int Print::visit_expr(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                           "print 语句不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
} 