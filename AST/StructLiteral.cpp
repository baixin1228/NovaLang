#include "StructLiteral.h"
#include "TypeChecker.h"

int StructLiteral::visit_stmt(VarType &result) {
    result = VarType::STRUCT;
    return 0;
}

int StructLiteral::visit_expr(VarType &result) {
    // 遍历所有字段，确保每个字段的值都是有效的表达式
    for (auto& field : fields) {
        VarType field_type;
        if (field.second->visit_expr(field_type) == -1) {
            return -1;
        }
    }
    
    result = VarType::STRUCT;
    return 0;
} 