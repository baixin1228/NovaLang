#include "Increment.h"
#include "TypeChecker.h"
#include "ASTParser.h"
#include <string>

int Increment::visit_stmt(VarType &result) {
    VarType var_type = lookup_var_type(var);
    if (var_type == VarType::NONE) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义的变量: " + var, line, __FILE__, __LINE__);
        return -1;
    }
    if (var_type != VarType::INT) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "无法对非整型变量递增: " + var, line, __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int Increment::visit_expr(VarType &result) {
    VarType var_type = lookup_var_type(var);
    if (var_type == VarType::NONE) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                               "未定义的变量: " + var, line, __FILE__, __LINE__);
        return -1;
    }
    if (var_type != VarType::INT) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                               "自增运算符只能用于整数类型", line, __FILE__, __LINE__);
        return -1;
    }
    result = VarType::INT;
    return 0;
} 