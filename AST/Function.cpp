#include "Function.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int Function::visit_stmt(VarType &result) {
    if (ctx.has_func(name)) {
        VarType return_type = VarType::NONE;
        auto &[param_types, ret_type] = ctx.get_func_type(name);

        // 设置参数类型
        for (size_t i = 0; i < params.size(); i++) {
            add_var_type(params[i], param_types[i], true);
        }
        
        // 处理函数体
        for (auto& stmt : body) {
            VarType stmt_type;
            int ret = stmt->visit_stmt(stmt_type);
            if (ret == -1) {
                return -1;
            }
            if (stmt_type != VarType::NONE) {
                return_type = stmt_type; // 最后一个 return 决定返回类型
            }
        }

        // 验证并更新返回类型
        if (ret_type != VarType::NONE && return_type != ret_type) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                "函数返回类型不匹配，期望: " + var_type_to_string(ret_type) + "，得到: " + var_type_to_string(return_type), line, __FILE__, __LINE__);
            return -1;
        } else if (return_type != VarType::NONE) {
            // 更新函数返回类型
            ctx.set_func_return_type(name, return_type);
        }

        result = return_type;
        return 0;
    } else {
        std::vector<VarType> param_types;
        for (auto& param : params) {
            param_types.push_back(VarType::NONE); // 类型待推断
        }
        
        VarType return_type = VarType::NONE;
        ctx.set_func_type(name, param_types, return_type);
        result = return_type;
        return 0;
    }
}

int Function::visit_expr(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "函数定义不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
} 