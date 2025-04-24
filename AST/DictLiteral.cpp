#include "DictLiteral.h"
#include "TypeChecker.h"
#include "StringLiteral.h"

int DictLiteral::visit_stmt(VarType &result) {
    result = VarType::DICT;
    return 0;
}

int DictLiteral::visit_expr(VarType &result) {
    if (items.empty()) {
        result = VarType::DICT;
        return 0;
    }
    
    // 确定键的类型
    VarType first_key_type;
    if (items[0].first->visit_expr(first_key_type) == -1) {
        return -1;
    }
    
    // 确保所有键都是字符串类型
    if (first_key_type != VarType::STRING) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                     "字典键必须是字符串类型，得到：" + var_type_to_string(first_key_type),
                     line, __FILE__, __LINE__);
        return -1;
    }
    
    key_type = first_key_type;
    
    // 确定值的类型
    VarType first_value_type;
    if (items[0].second->visit_expr(first_value_type) == -1) {
        return -1;
    }
    
    value_type = first_value_type;
    
    // 确保所有键和值都有合适的类型
    for (size_t i = 1; i < items.size(); ++i) {
        VarType key_type_i;
        if (items[i].first->visit_expr(key_type_i) == -1) {
            return -1;
        }
        
        if (key_type_i != key_type) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "字典键类型不一致，期望：" + var_type_to_string(key_type) + 
                         "，得到：" + var_type_to_string(key_type_i),
                         line, __FILE__, __LINE__);
            return -1;
        }
        
        VarType value_type_i;
        if (items[i].second->visit_expr(value_type_i) == -1) {
            return -1;
        }
        
        if (value_type_i != value_type) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "字典值类型不一致，期望：" + var_type_to_string(value_type) + 
                         "，得到：" + var_type_to_string(value_type_i),
                         line, __FILE__, __LINE__);
            return -1;
        }
    }
    
    result = VarType::DICT;
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