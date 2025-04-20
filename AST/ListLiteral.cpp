#include "ListLiteral.h"
#include "TypeChecker.h"

int ListLiteral::visit_stmt(VarType &result) {
    result = VarType::LIST;
    return 0;
}

int ListLiteral::visit_expr(VarType &result) {
    if (elements.empty()) {
        result = VarType::LIST;
        return 0;
    }
    
    // 确定元素类型
    VarType first_element_type;
    if (elements[0]->visit_expr(first_element_type) == -1) {
        return -1;
    }
    
    element_type = first_element_type;
    
    // 确保所有元素都有相同的类型
    for (size_t i = 1; i < elements.size(); ++i) {
        VarType element_type_i;
        if (elements[i]->visit_expr(element_type_i) == -1) {
            return -1;
        }
        
        if (element_type_i != element_type) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "列表元素类型不一致，期望：" + var_type_to_string(element_type) + 
                         "，得到：" + var_type_to_string(element_type_i),
                         line, __FILE__, __LINE__);
            return -1;
        }
    }
    
    result = VarType::LIST;
    return 0;
} 