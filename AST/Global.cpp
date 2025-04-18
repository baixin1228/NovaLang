#include "Global.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int Global::visit_stmt(VarType &result) {
    // 将变量标记为全局变量
    for (const auto& var : vars) {
        add_global_var(var);
    }
    result = VarType::NONE;
    return 0;
}

int Global::visit_expr(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "global语句不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
} 