#include "Call.h"
#include "ASTNode.h"
#include "Common.h"
#include "Context.h"
#include "Function.h"
#include "If.h"
#include "StructFieldAccess.h"
#include "StructLiteral.h"
#include "TypeChecker.h"
#include "Variable.h"
#include <execinfo.h>
#include <llvm-14/llvm/Support/raw_ostream.h>

int Call::visit_stmt() {
  std::shared_ptr<ASTNode> expr_ret_ast;
  return visit_expr(expr_ret_ast);
}

/* call like: ClassA() */
int Call::visit_class_expr(std::shared_ptr<ASTNode> &expr_ret,
                          std::shared_ptr<ASTNode> ast_node) {
  if (instance)
  {
    expr_ret = instance;
    return 0;
  }

  auto class_node = dynamic_cast<StructLiteral *>(ast_node.get());
  if (!class_node) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "非法访问，非类类型: " + name, line,
                  __FILE__, __LINE__);
    return -1;
  }
  // if (class_node->is_abstract) {
  //   ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "抽象类不能实例化: " + name, line,
  //                 __FILE__, __LINE__);
  //   return -1;
  // }
  instance = std::make_shared<StructLiteral>(*class_node);
  instance->type = VarType::INSTANCE;
  instance->functions.clear();
  instance->attributes.clear();

  type = instance->type;
  expr_ret = instance;
  return 0;
}

/* call like: func()/super() */
int Call::visit_func_expr(std::shared_ptr<ASTNode> &expr_ret,
                          std::shared_ptr<ASTNode> func_ast) {

  if (name == "super") {
    ASTNode *parent_ast = parent;
    while (parent_ast->type != VarType::CLASS &&
           parent_ast->type != VarType::INSTANCE && parent_ast->parent) {
      parent_ast = parent_ast->parent;
    }
    if (parent_ast->type == VarType::CLASS ||
        parent_ast->type == VarType::INSTANCE) {
      auto parent_class = dynamic_cast<StructLiteral *>(parent_ast);
      if (parent_class->class_parent_name.empty()) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, 
        "Class:" + name + " has no parent class, cannot call super", line,
                      __FILE__, __LINE__);
        return -1;
      }
      // expr_ret = parent_ast->shared_from_this();
    }
  }

  auto func_node = dynamic_cast<Function *>(func_ast.get());
  if (!func_node) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "调用非函数类型: " + name + " 得到类型：" +
                      var_type_to_string(func_ast->type) + " 行：" +
                      std::to_string(func_ast->line),
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
  if(func_node->visit_stmt() != 0) {
    return -1;
  }

  expr_ret = func_node->return_ast;
  if (expr_ret == nullptr) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "function:" + name + " has no return value",
                  line, __FILE__, __LINE__);
    return -1;
  }
  type = expr_ret->type;
  return 0;
}

/* call like: func() or ClassA() */
int Call::visit_func_or_class(std::shared_ptr<ASTNode> &expr_ret,
                          std::shared_ptr<ASTNode> ast_node) {
  if (!ast_node) {
    auto func_info = lookup_func(name);
    if (func_info) {
      ast_node = func_info->node;
      if (!ast_node) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                      "function:" + name + " is not resolved", line, __FILE__,
                      __LINE__);
        return -1;
      }
    }
  }
  if (!ast_node) {
    auto struct_info = lookup_struct(name);
    if (struct_info) {
      ast_node = struct_info->node;
    }
  }
  if (!ast_node) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义函数: " + name, line,
                  __FILE__, __LINE__);
    return -1;
  }
  if (ast_node->type == VarType::FUNCTION) {
    return visit_func_expr(expr_ret, ast_node);
  }
  if (ast_node->type == VarType::CLASS) {
    return visit_class_expr(expr_ret, ast_node);
  }
  ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "非法访问，非函数或类类型: " + name
                + " 类型: " + var_type_to_string(ast_node->type), line,
                __FILE__, __LINE__);
  return -1;
}

int Call::get_instance_func(Context &ctx, ASTNode &node, std::string class_name, 
                          std::string func_name, int line,
                          std::shared_ptr<ASTNode> &ret_ast) {
  while (true) {
    auto instance_class_ast = node.lookup_struct(class_name);
    if (!instance_class_ast) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义类: [" + class_name + "]",
                    line, __FILE__, __LINE__);
      return -1;
    }
    auto instance_class =
        std::dynamic_pointer_cast<StructLiteral>(instance_class_ast->node);
    auto func_ast = instance_class->functions.find(func_name);
    if (func_ast != instance_class->functions.end()) {
      ret_ast = func_ast->second;
      return 0;
    }
    if (instance_class->class_parent_name.empty()) {
      break;
    }
    class_name = instance_class->class_parent_name;
  }

  return -1;
}

int Call::visit_prev_expr(
    std::shared_ptr<ASTNode> &expr_ret, std::shared_ptr<ASTNode> &ret_ast) {
  if(return_ast)
  {
    ret_ast = return_ast;
    return 0;
  }

  std::shared_ptr<ASTNode> struct_instance;
  int ret = forward_expr->visit_expr(struct_instance);
  if (ret == -1) {
    return -1;
  }

  if (!struct_instance) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无效实例", line,
                  __FILE__, __LINE__);
    return -1;
  }

  auto struct_node = dynamic_cast<StructLiteral *>(struct_instance.get());
  if (!struct_node) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无效的结构体实例", line,
                  __FILE__, __LINE__);
    return -1;
  }
  
  if (struct_node->type == VarType::INSTANCE) {
    /* add self as first argument */
    args.insert(args.begin(), struct_instance);
    ret = get_instance_func(ctx, *this, struct_node->name, name, line, ret_ast);
    if (ret == -1) {
      return -1;
    }
    ret = visit_func_expr(expr_ret, ret_ast);
    if (ret == -1) {
      return -1;
    }
    /* add function to instance */
    auto value_ast = std::make_shared<Variable>(
        ctx, name, ret_ast->line);
    value_ast->set_parent(struct_node);
    struct_node->fields.push_back(
        std::make_pair(name, value_ast));
  }

  size_t field_offset = 0;
  VarType field_type = VarType::NONE;
  
  ret = access_field_offset(struct_instance, name, ret_ast, field_offset,
                            field_type);
  if (ret == -1) {
    return -1;
  }
  return_ast = ret_ast;

  return 0;
}

int Call::visit_expr(std::shared_ptr<ASTNode> &expr_ret_ast) {
  if (forward_expr) {
    std::shared_ptr<ASTNode> ret_ast;
    int ret = visit_prev_expr(expr_ret_ast, ret_ast);
    if (ret == -1) {
      return -1;
    }
    /* xxx.func() or xxx.ClassA() */
    return visit_func_or_class(expr_ret_ast, ret_ast);
  }

  /* func() */
  return visit_func_or_class(expr_ret_ast, nullptr);
}

int Call::gencode_stmt() {
  llvm::Value *dummy_value = nullptr;
  return gencode_expr(VarType::NONE, dummy_value);
}

/* call like: func() */
int Call::gencode_func_expr(VarType expected_type, llvm::Value *&ret_value) {
  std::vector<llvm::Value *> llvm_args;
  auto func_info = lookup_func(name);
  if (!func_info) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义函数: " + name, line,
                  __FILE__, __LINE__);
    return -1;
  }
  auto func_node = dynamic_cast<Function *>(func_info->node.get());
  if (!func_node) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "不是函数类型: " + name, line,
                  __FILE__, __LINE__);
    return -1;
  }

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

  if (func_node->return_ast->type != VarType::VOID) {
    ret_value = ctx.builder->CreateCall(func_info->llvm_func, llvm_args,
                                        "call_" + std::to_string(line));
  } else {
    ret_value = ctx.builder->CreateCall(func_info->llvm_func, llvm_args);
  }
  return 0;
}

int Call::gencode_call_expr(VarType expected_type, llvm::Value *&ret_value) {
  /* call like: ClassA() */
  auto class_info = lookup_struct(name);
  if (class_info) {
    int ret = instance->gencode_expr(expected_type, ret_value);
    if (ret == -1) {
      return -1;
    }
    return 0;
  }
  /* call like: func() */
  return gencode_func_expr(expected_type, ret_value);
}

/* call like: xxx.func() */
int Call::gencode_prev_expr(VarType expected_type, llvm::Value *&ret_value) {
  // generate struct value first
  llvm::Value *struct_val = nullptr;
  if (forward_expr->gencode_expr(VarType::STRUCT, struct_val) != 0) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法获取结构体实例", line,
                  __FILE__, __LINE__);
    return -1;
  }

  std::shared_ptr<ASTNode> ret_ast;
  std::shared_ptr<ASTNode> expr_ret_ast;
  int ret = visit_prev_expr(expr_ret_ast, ret_ast);
  if (ret == -1) {
    return -1;
    }
    auto func_node = dynamic_cast<Function *>(ret_ast.get());
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
    if (func_node->return_ast->type != VarType::VOID) {
      ret_value = ctx.builder->CreateCall(func_node->llvm_type, func_ptr,
                                          llvm_args, "struct_call_" + std::to_string(line));
    } else {
      ret_value = ctx.builder->CreateCall(func_node->llvm_type, func_ptr,
                                          llvm_args);
    }
    return 0;
 }

int Call::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
  if (forward_expr) {
    /* call like: xxx.func() */
    return gencode_prev_expr(expected_type, ret_value);
  }
  /* call like: func()/ClassA() */
  return gencode_call_expr(expected_type, ret_value);
}