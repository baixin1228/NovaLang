#include "Assign.h"
#include "Common.h"
#include "Context.h"
#include "StructLiteral.h"
#include "Variable.h"

int Assign::visit_stmt() {
  std::shared_ptr<ASTNode> value_ret;
  return visit_expr(value_ret);
}

int Assign::visit_expr(std::shared_ptr<ASTNode> &self) {
  std::shared_ptr<ASTNode> value_ret;
  int ret = value->visit_expr(value_ret);
  if (ret == -1) {
    return -1;
  }
  if (!value_ret) {
    ctx.print_errors();
    throw std::runtime_error(
        "值表达式未返回ASTNode: " + var + " source:" + std::to_string(line) +
        " file:" + std::string(__FILE__) + " line:" + std::to_string(__LINE__));
  }
  auto var_info = std::make_shared<VarInfo>();
  var_info->line = line;
  var_info->node = value_ret;
  if (add_var(var, var_info) == 0) {
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
    auto var_type = var_node->node->type;
    if (var_type != value_ret->type &&
        !(var_type == VarType::FLOAT && value_ret->type == VarType::INT)) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "变量：" + var + " 类型不匹配，得到的类型: " +
                        var_type_to_string(value_ret->type) + " 期望类型是：" +
                        var_type_to_string(var_type),
                    line, __FILE__, __LINE__);
      return -1;
    }
  }
  type = value_ret->type;
  self = value_ret;
  return 0;
}

/* 变量生成都在这里完成 */
int Assign::gencode_var() {
  auto var_info = lookup_var(var, line);
  if (!var_info) {
    throw std::runtime_error(
        "未定义的变量: " + var + " source:" + std::to_string(line) +
        " file:" + std::string(__FILE__) + " line:" + std::to_string(__LINE__));
  }
  if (var_info->node->type == VarType::NONE) {
    throw std::runtime_error(
        "变量类型为空: " + var + " source:" + std::to_string(line) +
        " file:" + std::string(__FILE__) + " line:" + std::to_string(__LINE__));
  }

  VarType type = var_info->node->type;

  if (need_create && var_info->llvm_obj == nullptr) {
    llvm::Type *ty;
    switch (type) {
    case VarType::INT:
      ty = ctx.builder->getInt64Ty();
      break;
    case VarType::FLOAT:
      ty = ctx.builder->getDoubleTy();
      break;
    case VarType::BOOL:
      ty = ctx.builder->getInt1Ty();
      break;
    case VarType::STRING:
    case VarType::STRUCT:
    case VarType::DICT:
    case VarType::LIST:
    case VarType::INSTANCE:
      ty = llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(),
                                  0);
      break;
    case VarType::CLASS:
      return 0;
    default:
      throw std::runtime_error("未定义的变量类型: " + var + " source:" +
                               std::to_string(line) + " file:" + std::string(__FILE__) +
                               " line:" + std::to_string(__LINE__));
      break;
    }

    if (is_global) {
      // 创建全局变量
      auto llvm_value =
          new llvm::GlobalVariable(*ctx.module, ty,
                                   false, // 是否为常量
                                   llvm::GlobalValue::ExternalLinkage,
                                   llvm::Constant::getNullValue(ty), // 初始值
                                   var);
      llvm_value->setAlignment(llvm::Align(get_type_align(type)));
      var_info->llvm_obj = llvm_value;
      std::cout << "==== Generating global variable: " << var
                << "====" << std::endl;
    } else {
      auto llvm_value = ctx.builder->CreateAlloca(ty, nullptr, var);
      llvm_value->setAlignment(llvm::Align(get_type_align(type)));
      var_info->llvm_obj = llvm_value;
      std::cout << "==== Generating local variable: " << var
                << "====" << std::endl;
    }
    return 0;
  } else {
    std::cout << "==== Generating not variable: " << var
              << " line:" << var_info->line << "====" << std::endl;
    return 0;
  }

  // if (auto *assign_value = dynamic_cast<Assign *>(value.get())) {
  //   if (assign_value->gencode_var(assign_value) == -1) {
  //     return -1;
  //   }
  // }
}

int Assign::gencode_assign(std::string name, std::shared_ptr<VarInfo> var_info,
                           std::shared_ptr<ASTNode> value,
                           llvm::Value *&ret_value) {
  if (!var_info->llvm_obj) {
    throw std::runtime_error(
        "变量未完成初始化: " + name + " code:" + std::to_string(var_info->line) +
        " file:" + std::string(__FILE__) + " line:" + std::to_string(__LINE__));
  }

  VarType var_type = var_info->node->type;
  int ret = value->gencode_expr(var_type, ret_value);
  if (ret == -1) {
    return -1;
  }
  if (!ret_value) {
    throw std::runtime_error(
        "变量未生成ir: " + name + " code:" + std::to_string(var_info->line) +
        " file:" + std::string(__FILE__) + " line:" + std::to_string(__LINE__));
  }

  // 如果是STRING类型（nova_memory_block），调用引用计数增加函数
  if (var_type == VarType::STRING) {
    // 先获取内存管理函数
    auto retain_func =
        value->ctx.runtime_manager->getRuntimeFunction("nova_memory_retain");
    if (!retain_func) {
      throw std::runtime_error("无法获取内存管理函数: nova_memory_retain" +
                               std::to_string(var_info->line) +
                               " line:" + std::to_string(__LINE__));
    }

    // 调用引用计数增加函数
    value->ctx.builder->CreateCall(retain_func, {ret_value});
  }

  auto str = value->ctx.builder->CreateStore(ret_value, var_info->llvm_obj);
  str->setAlignment(llvm::Align(get_type_align(var_type)));
  return 0;
}

int Assign::gencode_stmt() {
  llvm::Value *ret_value = nullptr;
  return gencode_expr(VarType::NONE, ret_value);
}

int Assign::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
  gencode_var();
  auto var_info = lookup_var(var, line);
  if (!var_info) {
    throw std::runtime_error(
        "未定义的变量: " + var + " source:" + std::to_string(line) +
        " file:" + std::string(__FILE__) + " line:" + std::to_string(__LINE__));
  }
  if (var_info->node->type == VarType::CLASS) {
    return 0;
  }
  return gencode_assign(var, var_info, value, ret_value);
}