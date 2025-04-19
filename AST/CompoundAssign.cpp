#include "CompoundAssign.h"
#include "ASTParser.h"
#include "TypeChecker.h"
#include <iostream>

void CompoundAssign::print(int indent) {
    std::cout << std::string(indent, ' ') << "CompoundAssign(" << var << " " << op << ")" << std::endl;
    value->print(indent + 2);
}

int CompoundAssign::visit_stmt(VarType &result) {
    VarType type;
    int ret = value->visit_expr(type);
    if (ret == -1) {
        return -1;
    }

    VarType existing_type = lookup_var_type(var);
    if (existing_type == VarType::NONE) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义的变量: " + var, line, __FILE__, __LINE__);
        return -1;
    }

    if (existing_type != type && !(existing_type == VarType::FLOAT && type == VarType::INT)) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "变量：" + var +
                    " 类型不匹配，得到的类型: " + var_type_to_string(type) +
                    " 期望类型是：" + var_type_to_string(existing_type),
                    line, __FILE__, __LINE__);
        return -1;
    }

    result = existing_type;
    return 0;
}

int CompoundAssign::visit_expr(VarType &result) {
    VarType type;
    int ret = value->visit_expr(type);
    if (ret == -1) {
        return -1;
    }

    VarType existing_type = lookup_var_type(var);
    if (existing_type == VarType::NONE) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义的变量: " + var, line, __FILE__, __LINE__);
        return -1;
    }

    if (existing_type != type && !(existing_type == VarType::FLOAT && type == VarType::INT)) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "类型不匹配: " + var + " 是 " + var_type_to_string(existing_type) +
                    " 类型，但赋值表达式是 " + var_type_to_string(type) + " 类型",
                    line, __FILE__, __LINE__);
        return -1;
    }

    result = existing_type;
    return 0;
} 