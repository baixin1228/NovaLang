#include "Variable.h"
#include "TypeChecker.h"
#include "ASTParser.h"
#include "Common.h"
#include "Call.h"
#include "StructFieldAccess.h"
#include "Function.h"

int Variable::visit_stmt() {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "变量引用不能作为语句使用", line, __FILE__, __LINE__);
    return -1;
}

int Variable::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
  if (parent && (parent->type == VarType::CLASS || parent->type == VarType::INSTANCE)) {
    auto instance = dynamic_cast<StructLiteral *>(parent);
    if (instance) {
      std::shared_ptr<ASTNode> func_ast;
      if (Call::get_instance_func(ctx, *this, instance->name, name, line,
                                  func_ast) == 0) {
        expr_ret = func_ast;
        type = func_ast->type;
        return 0;
      }
    }
  }

  auto var_info = lookup_var(name, line);
  if (var_info) {
      type = var_info->node->type;
      expr_ret = var_info->node;
    if (var_info->node->type == VarType::NONE) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义变量类型: " + name,
                    line, __FILE__, __LINE__);
      return -1;
    }
  } 
  std::shared_ptr<FuncInfo> func_info = nullptr;
  if(!var_info) {
    func_info = lookup_func(name);
    if (func_info) {
      expr_ret = func_info->node;
      type = expr_ret->type;
    }
  }
  std::shared_ptr<ClassInfo> struct_info = nullptr;
  if (!func_info) {
    struct_info = lookup_struct(name);
    if (struct_info) {
      type = struct_info->node->type;
      expr_ret = struct_info->node;
    }
  }

  if (!var_info && !func_info && !struct_info) {
    print_backtrace();
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义的变量: " + name, line,
                  __FILE__, __LINE__);
    return -1;
  }

  std::cout << "Variable::visit_expr: " << name << std::endl;
  return 0;
}

int Variable::gencode_stmt() { return 0; }

int Variable::gencode_var_expr(Context &ctx, std::string name,
                               int line,
                               VarType expected_type,
                               llvm::Value *&value,
                               std::shared_ptr<VarInfo> var_info) {
  auto ptr = var_info->llvm_obj;
  VarType type = var_info->node->type;
  // std::cout << "Variable::gencode_expr: " << name << " type: " <<
  // var_type_to_string(type) << std::endl;
  switch (type) {
  case VarType::STRING: {
    auto memory_block_ptr_type = llvm::PointerType::get(
        ctx.runtime_manager->getNovaMemoryBlockType(), 0);
    auto load =
        ctx.builder->CreateLoad(memory_block_ptr_type, ptr, name + "_load");
    load->setAlignment(llvm::Align(get_type_align(type)));
    value = load;
    return 0;
  }
  case VarType::INT: {
    auto load =
        ctx.builder->CreateLoad(ctx.builder->getInt64Ty(), ptr, name + "_load");
    load->setAlignment(llvm::Align(get_type_align(type)));
    if (expected_type != VarType::NONE && expected_type == VarType::FLOAT) {
      // 隐式类型转换
      value = ctx.builder->CreateSIToFP(load, ctx.builder->getDoubleTy());
      return 0;
    } else if (expected_type != VarType::NONE &&
               expected_type == VarType::BOOL) {
      // 隐式类型转换，0 为 False，非 0 为 True
      value = ctx.builder->CreateICmpNE(
          load, llvm::ConstantInt::get(ctx.builder->getInt64Ty(), 0));
      return 0;
    }
    value = load;
    return 0;
  }
  case VarType::FLOAT: {
    auto load = ctx.builder->CreateLoad(ctx.builder->getDoubleTy(), ptr,
                                        name + "_load");
    load->setAlignment(llvm::Align(get_type_align(type)));
    if (expected_type != VarType::NONE && expected_type == VarType::BOOL) {
      // 隐式类型转换，0.0 为 False，非 0.0 为 True
      value = ctx.builder->CreateFCmpUNE(
          load, llvm::ConstantFP::get(ctx.builder->getDoubleTy(), 0.0));
      return 0;
    }
    value = load;
    return 0;
  }
  case VarType::BOOL: {
    auto load =
        ctx.builder->CreateLoad(ctx.builder->getInt1Ty(), ptr, name + "_load");
    load->setAlignment(llvm::Align(get_type_align(type)));
    value = load;
    return 0;
  }
  case VarType::INSTANCE:
  case VarType::STRUCT: {
    // 对于结构体变量，加载结构体指针
    auto memory_block_ptr_type = llvm::PointerType::get(
        ctx.runtime_manager->getNovaMemoryBlockType(), 0);
    auto load =
        ctx.builder->CreateLoad(memory_block_ptr_type, ptr, name + "_load");
    load->setAlignment(llvm::Align(get_type_align(type)));
    value = load;
    return 0;
  }
  case VarType::DICT:
  case VarType::LIST: {
    // 其他复合类型，加载内存块指针
    auto memory_block_ptr_type = llvm::PointerType::get(
        ctx.runtime_manager->getNovaMemoryBlockType(), 0);
    auto load =
        ctx.builder->CreateLoad(memory_block_ptr_type, ptr, name + "_load");
    load->setAlignment(llvm::Align(get_type_align(type)));
    value = load;
    return 0;
  }
  default:
    throw std::runtime_error("加载数据失败，未知变量类型: " + name + " code:" +
                             std::to_string(line) + " file:" + __FILE__ +
                             " line:" + std::to_string(__LINE__));
    return -1;
  }
}

int Variable::gencode_expr(VarType expected_type, llvm::Value *&value) {
  /* a.func */
  if (parent && (parent->type == VarType::CLASS || parent->type == VarType::INSTANCE)) {
    auto instance = dynamic_cast<StructLiteral *>(parent);
    if (instance) {
      std::shared_ptr<ASTNode> func_ast;
      if (Call::get_instance_func(ctx, *this, instance->name, name, line, func_ast) == 0) {
        auto func_node = std::dynamic_pointer_cast<Function>(func_ast);
        if (func_node) {
          auto func_info = func_node->lookup_func(name);
          if (!func_info) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                          "类: " + instance->name + " 未定义方法: " + name, line, __FILE__,
                          __LINE__);
            return -1;
          }
          if (func_node->reference_count == 0) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                          "函数: " + name + "未完成类型推导", line, __FILE__,
                          __LINE__);
            return -1;
          }
          value = func_info->llvm_func;
          if (!value) {
            print_backtrace();
            throw std::runtime_error("function llvm_value is nullptr: " + name);
          }
          return 0;
        } else {
          throw std::runtime_error("function type error: " + name);
        }
      } else {
        throw std::runtime_error("function not found: " + name);
      }
    } else {
      throw std::runtime_error("class or instance not found: " + instance->name);
    }
  }

  /* a = b */
  auto var_info = lookup_var(name, line);
  if (!var_info) {
    /* a = func */
    auto func_info = lookup_func(name);
    if (func_info) {
      auto func_node = std::dynamic_pointer_cast<Function>(func_info->node);
      if (func_node) {
        if (func_node->reference_count == 0) {
          ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "函数: " + name + "未完成类型推导",
                        line, __FILE__, __LINE__);
          return -1;
        }
        value = func_info->llvm_func;
        return 0;
      } else {
        throw std::runtime_error("函数: " + name + "类型错误");
      }
    }
  }
  if (!var_info) {
    auto struct_info = lookup_struct(name);
    if (struct_info) {
      auto struct_node = std::dynamic_pointer_cast<StructLiteral>(struct_info->node);
      if (struct_node) {
        if (struct_node->struct_type == StructType::CLASS) {
          // if (struct_node->reference_count == 0) {
          //   ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
          //                 "类: " + name + "未完成类型推导", line, __FILE__,
          //                 __LINE__);
          //   return -1;
          // }
        } else {
          if (struct_node->reference_count == 0) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                          "结构体: " + name + "未完成类型推导", line, __FILE__,
                          __LINE__);
            return -1;
          }
        }

        // value = struct_node->llvm_obj;
        return 0;
      }
    }
  }
  if (!var_info) {
    throw std::runtime_error("未定义的变量: " + name +
                             " source:" + std::to_string(line) +
                             " file:" + std::string(__FILE__) +
                             " line:" + std::to_string(__LINE__));
    return -1;
  }
  return gencode_var_expr(ctx, name, line, expected_type, value, var_info);
}