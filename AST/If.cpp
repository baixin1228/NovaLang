#include "ASTParser.h"
#include "TypeChecker.h"
#include "If.h"

int If::visit_stmt(VarType &result) {
    VarType cond_type;
    int ret = condition->visit_expr(cond_type);
    if (ret == -1) {
        return -1;
    }
    if (cond_type != VarType::BOOL) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "if 条件必须是 bool 类型，得到: " + var_type_to_string(cond_type), line, __FILE__, __LINE__);
        return -1;
    }

    VarType return_type = VarType::NONE;
    for (auto& stmt : body) {
        VarType stmt_type;
        int ret = stmt->visit_stmt(stmt_type);
        if (ret == -1) {
            return -1;
        }
        if (stmt_type != VarType::NONE) {
            return_type = stmt_type;
        }
    }

    for (auto& elif : elifs) {
        VarType elif_cond_type;
        int ret = elif.first->visit_expr(elif_cond_type);
        if (ret == -1) {
            return -1;
        }
        if (elif_cond_type != VarType::BOOL) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                             "elif 条件必须是 bool 类型，得到: " + var_type_to_string(elif_cond_type), line, __FILE__, __LINE__);
            return -1;
        }
        for (auto& stmt : elif.second) {
            VarType stmt_type;
            int ret = stmt->visit_stmt(stmt_type);
            if (ret == -1) {
                return -1;
            }
            if (stmt_type != VarType::NONE) {
                return_type = stmt_type;
            }
        }
    }

    for (auto& stmt : else_body) {
        VarType stmt_type;
        int ret = stmt->visit_stmt(stmt_type);
        if (ret == -1) {
            return -1;
        }
        if (stmt_type != VarType::NONE) {
            return_type = stmt_type;
        }
    }

    result = return_type;
    return 0;
}

int If::visit_expr(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                           "if 语句不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
} 