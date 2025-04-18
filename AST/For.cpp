#include "For.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int For::visit_stmt(VarType &result) {
    VarType end_type;
    int ret = end->visit_expr(end_type);
    if (ret == -1) {
        return -1;
    }
    if (end_type != VarType::INT) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "for 循环范围必须是 int 类型，得到: " + var_type_to_string(end_type), line, __FILE__, __LINE__ );
        return -1;
    }
    add_var_type(iterator, VarType::INT, true);
    for (auto& stmt : body) {
        if (stmt) {
            VarType stmt_result;
            int ret = stmt->visit_stmt(stmt_result);
            if (ret == -1) {
                return -1;
            }
        }
    }
    return 0;
}

int For::visit_expr(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "for 循环不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
} 