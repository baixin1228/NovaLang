#include "Variable.h"
#include "TypeChecker.h"
#include "ASTParser.h"
#include "Common.h"

int Variable::visit_stmt() {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "变量引用不能作为语句使用", line, __FILE__, __LINE__);
    return -1;
}

int Variable::visit_expr(std::shared_ptr<ASTNode> &self) {
  auto ast_node = lookup_var(name, line);
  if (ast_node) {
    type = ast_node->type;
    self = ast_node;
    if (ast_node->type == VarType::NONE) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义变量类型: " + name,
                    line, __FILE__, __LINE__);
      return -1;
    }
  } else {
    ast_node = lookup_func(name);
    if (ast_node) {
      type = VarType::FUNCTION;
      self = shared_from_this();
    }
  }

  if (!ast_node) {
    throw std::runtime_error("未定义的变量: " + name +
                              " source:" + std::to_string(line) +
                              " file:" + std::string(__FILE__) +
                              " line:" + std::to_string(__LINE__));
    return -1;
  }
  // print_backtrace();
  // std::cout << "Variable::visit_expr: " << name << " type: " <<
  // var_type_to_string(ast_node->type) << std::endl;
  return 0;
}

int Variable::gencode_stmt() { return 0; }

int Variable::gencode_expr(VarType expected_type, llvm::Value *&value) {
  auto var_node = lookup_var(name, line);
  if (!var_node) {
    var_node = lookup_func(name);
    if (var_node) {
      auto func_node = std::dynamic_pointer_cast<Function>(var_node);
      if (func_node) {
        if (func_node->reference_count == 0) {
          ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "函数: " + name + "未完成类型推导",
                        line, __FILE__, __LINE__);
          return -1;
        }
        value = func_node->llvm_obj;
        return 0;
      }
    }
  }
  if (!var_node) {
    throw std::runtime_error("未定义的变量: " + name +
                             " source:" + std::to_string(line) +
                             " file:" + std::string(__FILE__) +
                             " line:" + std::to_string(__LINE__));
    return -1;
  }
  auto ptr = var_node->llvm_obj;
  VarType type = var_node->type;
  // std::cout << "Variable::gencode_expr: " << name << " type: " << var_type_to_string(type) << std::endl;
  switch (type) {
  case VarType::STRING: {
    auto memory_block_ptr_type =
        llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0);
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
    } else if (expected_type != VarType::NONE && expected_type == VarType::BOOL) {
      // 隐式类型转换，0 为 False，非 0 为 True
      value = ctx.builder->CreateICmpNE(load, llvm::ConstantInt::get(ctx.builder->getInt64Ty(), 0));
      return 0;
    }
    value = load;
    return 0;
  }
  case VarType::FLOAT: {
    auto load =
        ctx.builder->CreateLoad(ctx.builder->getDoubleTy(), ptr, name + "_load");
    load->setAlignment(llvm::Align(get_type_align(type)));
    if (expected_type != VarType::NONE && expected_type == VarType::BOOL) {
      // 隐式类型转换，0.0 为 False，非 0.0 为 True
      value = ctx.builder->CreateFCmpUNE(load, llvm::ConstantFP::get(ctx.builder->getDoubleTy(), 0.0));
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
  case VarType::STRUCT: {
    // 对于结构体变量，加载结构体指针
    auto memory_block_ptr_type =
        llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0);
    auto load =
        ctx.builder->CreateLoad(memory_block_ptr_type, ptr, name + "_load");
    load->setAlignment(llvm::Align(get_type_align(type)));
    value = load;
    return 0;
  }
  case VarType::DICT:
  case VarType::LIST: {
    // 其他复合类型，加载内存块指针
    auto memory_block_ptr_type =
        llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0);
    auto load =
        ctx.builder->CreateLoad(memory_block_ptr_type, ptr, name + "_load");
    load->setAlignment(llvm::Align(get_type_align(type)));
    value = load;
    return 0;
  }
  default:
    throw std::runtime_error("加载数据失败，未知变量类型: " + name +
                             " code:" + std::to_string(line) + " file:" +
                             __FILE__ + " line:" + std::to_string(__LINE__));
    return -1;
  }
}