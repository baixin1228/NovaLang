#include "Print.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int Print::visit_stmt(VarType &result) {
    // 检查每个表达式的类型
    for (const auto& value : values) {
        VarType expr_type;
        if (value->visit_expr(expr_type) == -1) {
            return -1;
        }
    }
    result = VarType::VOID;
    return 0;
}

int Print::visit_expr(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "print语句不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
} 