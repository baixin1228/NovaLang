#include "ListLiteral.h"
#include "TypeChecker.h"

int ListLiteral::visit_stmt() {
    type = VarType::LIST;
    return 0;
}

int ListLiteral::visit_expr(std::shared_ptr<ASTNode> &self) {
    if (elements.empty()) {
        type = VarType::LIST;
        return 0;
    }
    
    // 确定元素类型
    VarType first_element_type;
    std::shared_ptr<ASTNode> first_element_ast;
    if (elements[0]->visit_expr(first_element_ast) == -1) {
        return -1;
    }

    element_type = first_element_ast->type;

    // 确保所有元素都有相同的类型
    for (size_t i = 1; i < elements.size(); ++i) {
        std::shared_ptr<ASTNode> element_ast;
        if (elements[i]->visit_expr(element_ast) == -1) {
            return -1;
        }
        
        if (element_ast->type != element_type) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "列表元素类型不一致，期望：" + var_type_to_string(element_type) + 
                         "，得到：" + var_type_to_string(element_ast->type),
                         line, __FILE__, __LINE__);
            return -1;
        }
    }
    
    type = VarType::LIST;
    self = shared_from_this();
    return 0;
}

int ListLiteral::gencode_stmt() { return 0; }

int ListLiteral::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
  // 列表实现将在后续完成，目前简单返回null指针并报告错误
  ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "列表字面量暂不支持代码生成",
                line, __FILE__, __LINE__);
  ret_value = llvm::ConstantPointerNull::get(
      llvm::PointerType::get(ctx.builder->getInt8Ty(), 0));
  return -1; // 返回-1表示生成失败
}