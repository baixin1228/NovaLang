#include "DictLiteral.h"
#include "TypeChecker.h"
#include "StringLiteral.h"

int DictLiteral::visit_stmt() {
    return 0;
}

int DictLiteral::visit_expr(std::shared_ptr<ASTNode> &self) {
    if (items.empty()) {
        return 0;
    }
    
    // 确定键的类型
    std::shared_ptr<ASTNode> key_ast;
    if (items[0].first->visit_expr(key_ast) == -1) {
        return -1;
    }
    
    // 确保所有键都是字符串类型
    if (key_ast->type != VarType::STRING) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                     "字典键必须是字符串类型，得到：" + var_type_to_string(key_ast->type),
                     line, __FILE__, __LINE__);
        return -1;
    }
    
    key_type = key_ast->type;
    
    // 确定值的类型
    std::shared_ptr<ASTNode> value_ast;
    if (items[0].second->visit_expr(value_ast) == -1) {
        return -1;
    }

    value_type = value_ast->type;

    // 确保所有键和值都有合适的类型
    for (size_t i = 1; i < items.size(); ++i) {
        std::shared_ptr<ASTNode> item_key_ast;
        if (items[i].first->visit_expr(item_key_ast) == -1) {
            return -1;
        }
        
        if (item_key_ast->type != key_type) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "字典键类型不一致，期望：" + var_type_to_string(key_type) + 
                         "，得到：" + var_type_to_string(item_key_ast->type),
                         line, __FILE__, __LINE__);
            return -1;
        }

        std::shared_ptr<ASTNode> item_value_ast;
        if (items[i].second->visit_expr(item_value_ast) == -1) {
          return -1;
        }

        if (item_value_ast->type != value_type) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "字典值类型不一致，期望：" + var_type_to_string(value_type) + 
                         "，得到：" + var_type_to_string(item_value_ast->type),
                         line, __FILE__, __LINE__);
            return -1;
        }
    }
    
    type = VarType::DICT;
    self = shared_from_this();
    return 0;
}

int DictLiteral::gencode_stmt() { return 0; }

llvm::Value *DictLiteral::gencode_expr(VarType expected_type) {
  // 字典实现将在后续完成，目前简单返回null指针
  ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "字典字面量暂不支持代码生成",
                line, __FILE__, __LINE__);
  return llvm::ConstantPointerNull::get(
      llvm::PointerType::get(ctx.builder->getInt8Ty(), 0));
}