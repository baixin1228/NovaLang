#include "FloatLiteral.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int FloatLiteral::visit_stmt(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "浮点数字面量不能作为语句使用", line, __FILE__, __LINE__);
    return -1;
}

int FloatLiteral::visit_expr(VarType &result) {
    result = VarType::FLOAT;
    return 0;
} 