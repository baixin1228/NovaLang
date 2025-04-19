#include "StringLiteral.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int StringLiteral::visit_stmt(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "字符串字面量不能作为语句使用", line, __FILE__, __LINE__);
    return -1;
}

int StringLiteral::visit_expr(VarType &result) {
    result = VarType::STRING;
    return 0;
} 