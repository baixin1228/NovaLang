#include "Variable.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int Variable::visit_stmt(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "变量引用不能作为语句使用", line, __FILE__, __LINE__);
    return -1;
}

int Variable::visit_expr(VarType &result) {
    VarType type = lookup_var_type(name);
    if (type == VarType::NONE) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                               "未定义的变量: " + name, line, __FILE__, __LINE__);
        return -1;
    }
    result = type;
    return 0;
}