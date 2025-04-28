#include "Assign.h"
#include "Common.h"
#include "Context.h"
#include "StructLiteral.h"
#include "Variable.h"

int Assign::visit_stmt() {
  std::shared_ptr<ASTNode> value_ast;
  int ret = value->visit_expr(value_ast);
  if (ret == -1) {
    return -1;
  }
  if (!value_ast) {
    ctx.print_errors();
    throw std::runtime_error("未返回ASTNode: " + var +
                             " source:" + std::to_string(line) +
                             " file:" + std::string(__FILE__) +
                             " line:" + std::to_string(__LINE__));
  }
  if (add_var(var, value_ast) == 0) {
    need_create = true;
    if (get_scope_depth() == 0) {
      is_global = true;
    }
  } else {
    auto var_node = lookup_var(var, line);
    if (!var_node) { 
      throw std::runtime_error("未定义的变量: " + var +
                               " source:" + std::to_string(line) +
                               " file:" + std::string(__FILE__) +
                               " line:" + std::to_string(__LINE__));
    }
    auto var_type = var_node->type;
    if (var_type != value_ast->type &&
        !(var_type == VarType::FLOAT && value_ast->type == VarType::INT)) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "变量：" + var + " 类型不匹配，得到的类型: " +
                        var_type_to_string(value_ast->type) + " 期望类型是：" +
                        var_type_to_string(var_type),
                    line, __FILE__, __LINE__);
      return -1;
    }
  }
  type = value_ast->type;
  return 0;
}

int Assign::visit_expr(std::shared_ptr<ASTNode> &self) {
  // Regular variable assignment
  std::shared_ptr<ASTNode> value_ast;
  int ret = value->visit_expr(value_ast);
  if (ret == -1) {
    return -1;
  }

  if (add_var(var, value_ast) == 0) {
    auto var_info = dynamic_cast<Variable*>(lookup_var(var, line).get());
    var_info->type = value_ast->type;
    need_create = true;
    if (get_scope_depth() == 0) {
      is_global = true;
    }
  } else {
    auto var_info = dynamic_cast<Variable*>(lookup_var(var, line).get());
    if (var_info->type != value_ast->type &&
        !(var_info->type == VarType::FLOAT && value_ast->type == VarType::INT)) {
      ctx.add_error(
          ErrorHandler::ErrorLevel::TYPE,
          "类型不匹配: " + var + " 是 " + var_type_to_string(var_info->type) +
              " 类型，但赋值表达式是 " + var_type_to_string(value_ast->type) + " 类型",
          line, __FILE__, __LINE__);
      return -1;
    }
  }
  return 0;
}

int Assign::gencode_stmt() {
  auto var_node = lookup_var(var, line);
  VarType type = var_node->type;
  if (type == VarType::NONE) {
    throw std::runtime_error("未定义的变量: " + var +
                           " source:" + std::to_string(line) +
                           " file:" + std::string(__FILE__) +
                           " line:" + std::to_string(__LINE__));
  }

  auto ptr = var_node->llvm_obj;
  if (!ptr) {
    throw std::runtime_error(
        "未定义的变量: " + var + " code:" + std::to_string(line) +
        " file:" + std::string(__FILE__) + " line:" + std::to_string(__LINE__));
  }
  llvm::Value* llvm_value = nullptr;
  int ret = value->gencode_expr(type, llvm_value);
  if (ret == -1) {
    return -1;
  }

  // 如果是STRING类型（nova_memory_block），调用引用计数增加函数
  if (type == VarType::STRING) {
    // 先获取内存管理函数
    auto retain_func =
        ctx.runtime_manager->getRuntimeFunction("nova_memory_retain");
    if (!retain_func) {
      throw std::runtime_error("无法获取内存管理函数: nova_memory_retain" +
                             std::to_string(line) +
                             " line:" + std::to_string(__LINE__));
    }

    // 调用引用计数增加函数
    ctx.builder->CreateCall(retain_func, {llvm_value});
  }

  auto str = ctx.builder->CreateStore(llvm_value, ptr);
  str->setAlignment(llvm::Align(get_type_align(type)));
  return 0;
}

int Assign::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
  // The assignment expression itself should return the assigned value.
  // We get the value from the 'value' node.
  return value->gencode_expr(expected_type, ret_value);
}