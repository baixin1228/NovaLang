#include "Call.h"
#include "Context.h"
#include "Function.h"
#include "StructFieldAccess.h"
#include "StructLiteral.h"
#include "TypeChecker.h"
#include "Variable.h"
#include <execinfo.h>

int Call::visit_stmt() {
  std::shared_ptr<ASTNode> self_ast;
  return visit_expr(self_ast);
}

int Call::visit_func_expr(std::shared_ptr<ASTNode> &self,
                          std::shared_ptr<ASTNode> ast_node) {
  if (!ast_node) {
    ast_node = lookup_func(name);
  }
  if (!ast_node) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义函数: " + name, line,
                  __FILE__, __LINE__);
    return -1;
  }

  auto func_node = dynamic_cast<Function *>(ast_node.get());
  if (!func_node) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "调用非函数类型: " + name + " 得到类型：" +
                      var_type_to_string(ast_node->type) + " 行：" +
                      std::to_string(ast_node->line),
                  line, __FILE__, __LINE__);
    return -1;
  }

  if (args.size() != func_node->params.size()) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "参数数量不匹配: 期望 " +
                      std::to_string(func_node->params.size()) + ", 得到 " +
                      std::to_string(args.size()),
                  line, __FILE__, __LINE__);
    return -1;
  }

  // update params type
  for (size_t i = 0; i < args.size(); i++) {
    std::shared_ptr<ASTNode> arg_ast;
    int ret = args[i]->visit_expr(arg_ast);
    if (ret == -1) {
      return -1;
    }
    if (func_node->params[i].second &&
        func_node->params[i].second->type != arg_ast->type) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "参数类型不匹配: 期望 " +
                        var_type_to_string(func_node->params[i].second->type) +
                        ", 得到 " + var_type_to_string(arg_ast->type),
                    line, __FILE__, __LINE__);
      return -1;
    }
    func_node->params[i].second = arg_ast;
  }
  func_node->reference_count++;
  func_node->visit_stmt();

  self = func_node->return_ast;
  type = func_node->type;
  return 0;
}

int Call::visit_struct_expr(
    std::shared_ptr<ASTNode> &self, std::shared_ptr<ASTNode> &field_value) {
  // Implementation for struct methods

  // Evaluate the forward_expr to get the struct instance
  std::shared_ptr<ASTNode> struct_instance;
  int ret = forward_expr->visit_expr(struct_instance);
  if (ret == -1) {
    return -1;
  }

  if (!struct_instance) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无效的结构体实例", line,
                  __FILE__, __LINE__);
    return -1;
  }

  auto struct_node = dynamic_cast<StructLiteral *>(struct_instance.get());
  if (!struct_node) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无效的结构体实例", line,
                  __FILE__, __LINE__);
    return -1;
  }

  size_t field_offset = 0;
  VarType field_type = VarType::NONE;
  
  ret = access_field_offset(struct_instance, name, field_value, field_offset,
                            field_type);
  if (ret == -1) {
    return -1;
  }
  return 0;
}

int Call::visit_expr(std::shared_ptr<ASTNode> &self_ast) {
  // Check if this is a method call on an object
  if (forward_expr) {
    std::shared_ptr<ASTNode> field_value;
    int ret = visit_struct_expr(self_ast, field_value);
    if (ret == -1) {
      return -1;
    }
    return visit_func_expr(self_ast, field_value);
  }

  // Otherwise, treat it as a regular function call
  return visit_func_expr(self_ast, nullptr);
}

int Call::gencode_stmt() {
  llvm::Value *dummy_value =
      nullptr; // Dummy variable for the reference parameter
  return gencode_expr(VarType::NONE, dummy_value);
}

int Call::gencode_func_expr(VarType expected_type, llvm::Value *&ret_value) {
  // 查找函数
  auto ast_node = lookup_func(name);
  if (!ast_node) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义函数: " + name, line,
                  __FILE__, __LINE__);
    return -1;
  }

  auto func_node = dynamic_cast<Function *>(ast_node.get());
  if (!func_node) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "不是函数类型: " + name, line,
                  __FILE__, __LINE__);
    return -1;
  }
  auto func_value = func_node->llvm_obj;
  // 准备参数列表
  std::vector<llvm::Value *> llvm_args;

  // 处理所有参数
  for (auto &arg : args) {
    llvm::Value *arg_val = nullptr;
    if (arg->gencode_expr(VarType::NONE, arg_val) == -1) {
      return -1;
    }
    if (!arg_val) {
      ctx.add_error(ErrorHandler::ErrorLevel::INTERNAL,
                    "生成参数代码失败 for function call " + name, line,
                    __FILE__, __LINE__);
      return -1;
    }
    llvm_args.push_back(arg_val);
  }
  
  // 调用函数
  ret_value = ctx.builder->CreateCall(func_value, llvm_args, "call");
  return 0;
}

int Call::gencode_struct_expr(VarType expected_type, llvm::Value *&ret_value) {
    std::shared_ptr<ASTNode> field_value;
    std::shared_ptr<ASTNode> self_ast;
    int ret = visit_struct_expr(self_ast, field_value);
    if (ret == -1) {
      return -1;
    }
    auto func_node = dynamic_cast<Function *>(field_value.get());
    if (!func_node) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "不是函数类型: " + name, line,
                    __FILE__, __LINE__);
      return -1;
    }
    if(!func_node->llvm_type) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "函数未生成: " + name, line,
                    __FILE__, __LINE__);
      return -1;
    }

    // 获取结构体对象
    llvm::Value *struct_val = nullptr;
    if (forward_expr->gencode_expr(VarType::STRUCT, struct_val) != 0) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法获取结构体实例", line,
                  __FILE__, __LINE__);
      return -1;
    }
    
    // 获取结构体数据区域指针
    auto data_ptr = ctx.builder->CreateCall(
        ctx.runtime_manager->getRuntimeFunction("nova_memory_get_data"),
        {struct_val});
    auto byte_ptr = ctx.builder->CreateBitCast(
        data_ptr, llvm::PointerType::get(ctx.builder->getInt8Ty(), 0));
    
    // 获取结构体实例以获取字段偏移量
    std::shared_ptr<ASTNode> struct_instance;
    if (forward_expr->visit_expr(struct_instance) != 0 || !struct_instance) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无效的结构体实例", line,
                  __FILE__, __LINE__);
      return -1;
    }
    
    // 计算字段偏移量
    size_t field_offset = 0;
    VarType field_type = VarType::NONE;
    std::shared_ptr<ASTNode> method_node;
    
    // 获取字段偏移量
    if (access_field_offset(struct_instance, name, method_node, field_offset, field_type) != 0) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE, 
                  "无法找到结构体字段: " + name, line,
                  __FILE__, __LINE__);
      return -1;
    }
    
    // 计算字段指针位置
    auto field_ptr = ctx.builder->CreateGEP(
        ctx.builder->getInt8Ty(), byte_ptr,
        llvm::ConstantInt::get(ctx.builder->getInt64Ty(), field_offset));
    
    // 将字段指针转换为函数指针类型
    auto func_ptr_type = llvm::PointerType::get(func_node->llvm_type, 0);
    auto func_ptr = ctx.builder->CreateBitCast(field_ptr, func_ptr_type);
    
    // 加载函数指针
    func_ptr = ctx.builder->CreateLoad(func_ptr_type, func_ptr);

    std::vector<llvm::Value *> llvm_args;
    // 处理所有参数
    for (auto &arg : args) {
      llvm::Value *arg_val = nullptr;
      if (arg->gencode_expr(VarType::NONE, arg_val) == -1) {
        return -1;
      }
      if (!arg_val) {
        ctx.add_error(ErrorHandler::ErrorLevel::INTERNAL,
                      "生成参数代码失败 for function call " + name, line,
                      __FILE__, __LINE__);
        return -1;
      }
      llvm_args.push_back(arg_val);
    }
    
    // 调用函数指针
    ret_value = ctx.builder->CreateCall(func_node->llvm_type, func_ptr, llvm_args, "struct_call");
    return 0;
 }

int Call::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
  // 如果存在前向表达式，则调用结构体方法
  if (forward_expr) {
    return gencode_struct_expr(expected_type, ret_value);
  }
  
  // 否则调用普通函数
  return gencode_func_expr(expected_type, ret_value);
}