#include "IntLiteral.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int IntLiteral::visit_stmt(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "整数字面量不能作为语句使用", line, __FILE__, __LINE__);
    return -1;
}

int IntLiteral::visit_expr(VarType &result) {
    result = VarType::INT;
    return 0;
} 