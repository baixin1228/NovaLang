#include "Increment.h"
#include "Common.h"
#include "Context.h"
#include <string>
#include "Variable.h"

int Increment::visit_stmt() {
    auto var_info = dynamic_cast<Variable*>(lookup_var(var, line)->node.get());
    if (var_info->type == VarType::NONE) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义的变量: " + var, line, __FILE__, __LINE__);
        return -1;
    }
    if (var_info->type != VarType::INT) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "无法对非整型变量递增: " + var, line, __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int Increment::visit_expr(std::shared_ptr<ASTNode> &self) {
    auto var_info = dynamic_cast<Variable*>(lookup_var(var, line)->node.get());
    if (var_info->type == VarType::NONE) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                               "未定义的变量: " + var, line, __FILE__, __LINE__);
        return -1;
    }
    if (var_info->type != VarType::INT) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                               "自增运算符只能用于整数类型", line, __FILE__, __LINE__);
        return -1;
    }
    type = var_info->type;
    return 0;
} 

int Increment::gencode_stmt() {
  auto var_info = lookup_var(var, line);
  if (!var_info) {
    throw std::runtime_error("未定义的变量: " + var +
                             " source:" + std::to_string(line) +
                             " file:" + std::string(__FILE__) +
                             " line:" + std::to_string(__LINE__));
  }
  auto ptr = var_info->llvm_obj;
  VarType type = var_info->node->type;
  auto old_val =
      ctx.builder->CreateLoad(ctx.builder->getInt64Ty(), ptr, var + "_old");
  old_val->setAlignment(llvm::Align(get_type_align(type)));
  auto new_val =
      ctx.builder->CreateAdd(old_val, ctx.builder->getInt64(1), var + "_inc");
  auto str = ctx.builder->CreateStore(new_val, ptr);
  str->setAlignment(llvm::Align(get_type_align(type)));
  return 0;
}

int Increment::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
  auto var_info = lookup_var(var, line);
  if (!var_info) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义的变量: " + var, line, __FILE__, __LINE__);
    return -1;
  }
  auto ptr = var_info->llvm_obj;
  VarType type = var_info->node->type;
  if (type != VarType::INT) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法对非整型变量递增: " + var, line, __FILE__, __LINE__);
    return -1;
  }
  
  auto old_val = ctx.builder->CreateLoad(ctx.builder->getInt64Ty(), ptr, var + "_old");
  old_val->setAlignment(llvm::Align(get_type_align(type)));
  auto new_val = ctx.builder->CreateAdd(old_val, ctx.builder->getInt64(1), var + "_inc");
  auto str = ctx.builder->CreateStore(new_val, ptr);
  str->setAlignment(llvm::Align(get_type_align(type)));
  
  // 自增表达式返回递增后的值
  ret_value = new_val;
  return 0;
}