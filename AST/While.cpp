#include "While.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int While::visit_stmt(VarType &result) {
    VarType cond_type;
    int ret = condition->visit_expr(cond_type);
    if (ret == -1) {
        return -1;
    }
    if (cond_type != VarType::BOOL) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "while 条件必须是 bool 类型，得到: " + var_type_to_string(cond_type), line, __FILE__, __LINE__);
        return -1;
    }
    for (auto& stmt : body) {
        if (stmt) {
            VarType stmt_type;
            int ret = stmt->visit_stmt(stmt_type);
            if (ret == -1) {
                return -1;
            }
        }
    }
    return 0;
}

int While::visit_expr(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "while 循环不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
} 