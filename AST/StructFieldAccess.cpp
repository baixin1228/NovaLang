#include "StructFieldAccess.h"
#include "StructLiteral.h"
#include "Variable.h"
#include "Context.h"

int StructFieldAccess::visit_stmt(VarType &result) {
    return visit_expr(result);
}

int StructFieldAccess::visit_expr(VarType &result) {
    auto* var_lit = dynamic_cast<Variable*>(struct_expr.get());
    auto struct_var = ctx.lookup_global_struct(var_lit->name);
    if (!struct_var) {
        throw std::runtime_error("未定义的结构体: " + var_lit->name + " code:" + std::to_string(line) + " file:" + __FILE__ + " line:" + std::to_string(__LINE__));
        return -1;
    }
    auto* struct_lit = dynamic_cast<StructLiteral*>(struct_var.get());
    auto filed = struct_lit->fields.find(field_name);
    if (filed == struct_lit->fields.end()) {
        throw std::runtime_error("未定义的结构体字段: " + field_name + " code:" + std::to_string(line) + " file:" + __FILE__ + " line:" + std::to_string(__LINE__));
        return -1;
    }
    return filed->second->visit_expr(result);
}