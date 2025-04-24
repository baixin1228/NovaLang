#include "Variable.h"
#include "TypeChecker.h"
#include "ASTParser.h"
#include "Common.h"

int Variable::visit_stmt(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "变量引用不能作为语句使用", line, __FILE__, __LINE__);
    return -1;
}

int Variable::visit_expr(VarType &result) {
    auto& var_info = lookup_var_info(name);
    if (var_info.type == VarType::NONE) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                               "未定义的变量: " + name, line, __FILE__, __LINE__);
        return -1;
    }
    result = var_info.type;
    return 0;
}

int Variable::gencode_stmt() { return 0; }

llvm::Value *Variable::gencode_expr(VarType expected_type) {
  auto ptr = lookup_var_llvm_obj(name);
  if (!ptr) {
    throw std::runtime_error("未定义的变量: " + name +
                             " source:" + std::to_string(line) +
                             " file:" + std::string(__FILE__) +
                             " line:" + std::to_string(__LINE__));
    return nullptr;
  }
  auto &var_info = lookup_var_info(name);
  VarType type = var_info.type;
  switch (type) {
  case VarType::STRING: {
    auto memory_block_ptr_type =
        llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0);
    auto load =
        ctx.builder->CreateLoad(memory_block_ptr_type, ptr, name + "_load");
    load->setAlignment(llvm::Align(get_type_align(type)));
    return load;
  }
  case VarType::INT: {
    auto load =
        ctx.builder->CreateLoad(ctx.builder->getInt64Ty(), ptr, name + "_load");
    load->setAlignment(llvm::Align(get_type_align(type)));
    if (expected_type != VarType::NONE && expected_type == VarType::FLOAT) {
      // 隐式类型转换
      return ctx.builder->CreateSIToFP(load, ctx.builder->getDoubleTy());
    } else if (expected_type != VarType::NONE && expected_type == VarType::BOOL) {
      // 隐式类型转换，0 为 False，非 0 为 True
      return ctx.builder->CreateICmpNE(load, llvm::ConstantInt::get(ctx.builder->getInt64Ty(), 0));
    }
    return load;
  }
  case VarType::FLOAT: {
    auto load =
        ctx.builder->CreateLoad(ctx.builder->getDoubleTy(), ptr, name + "_load");
    load->setAlignment(llvm::Align(get_type_align(type)));
    if (expected_type != VarType::NONE && expected_type == VarType::BOOL) {
      // 隐式类型转换，0.0 为 False，非 0.0 为 True
      return ctx.builder->CreateFCmpUNE(load, llvm::ConstantFP::get(ctx.builder->getDoubleTy(), 0.0));
    }
    return load;
  }
  case VarType::BOOL: {
    auto load =
        ctx.builder->CreateLoad(ctx.builder->getInt1Ty(), ptr, name + "_load");
    load->setAlignment(llvm::Align(get_type_align(type)));
    return load;
  }
  case VarType::STRUCT: {
    // 对于结构体变量，加载结构体指针
    auto memory_block_ptr_type =
        llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0);
    auto load =
        ctx.builder->CreateLoad(memory_block_ptr_type, ptr, name + "_load");
    load->setAlignment(llvm::Align(get_type_align(type)));
    return load;
  }
  case VarType::DICT:
  case VarType::LIST: {
    // 其他复合类型，加载内存块指针
    auto memory_block_ptr_type =
        llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0);
    auto load =
        ctx.builder->CreateLoad(memory_block_ptr_type, ptr, name + "_load");
    load->setAlignment(llvm::Align(get_type_align(type)));
    return load;
  }
  default:
    throw std::runtime_error("加载数据失败，未知变量类型: " + name +
                             " code:" + std::to_string(line) + " file:" +
                             __FILE__ + " line:" + std::to_string(__LINE__));
    return nullptr;
  }
}