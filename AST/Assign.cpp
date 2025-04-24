#include "Assign.h"
#include "Common.h"
#include "Context.h"

int Assign::visit_stmt(VarType &result) {
  VarType type;
  int ret = value->visit_expr(type);
  if (ret == -1) {
    return -1;
  }

  std::cout << "Assign::visit_stmt: " << var << " " << var_type_to_string(type)
            << std::endl;
  if (add_var_info(var) == 0) {
    auto &var_info = lookup_var_info(var);
    var_info.type = type;
    need_create = true;
    if (get_scope_depth() == 0) {
      is_global = true;
    }
  } else {
    auto &var_info = lookup_var_info(var);
    if (var_info.type != type &&
        !(var_info.type == VarType::FLOAT && type == VarType::INT)) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "变量：" + var +
                        " 类型不匹配，得到的类型: " + var_type_to_string(type) +
                        " 期望类型是：" + var_type_to_string(var_info.type),
                    line, __FILE__, __LINE__);
      return -1;
    }
  }
  if (type == VarType::STRUCT) {
    ctx.add_global_struct(var, value);
  }
  result = type;
  std::cout << "Assign::visit_stmt: " << var << " " << var_type_to_string(type) << std::endl;
  return 0;
}

int Assign::visit_expr(VarType &result) {
  int ret = value->visit_expr(result);
  if (ret == -1) {
    return -1;
  }

  if (add_var_info(var) == 0) {
    auto &var_info = lookup_var_info(var);
    var_info.type = result;
    need_create = true;
    if (get_scope_depth() == 0) {
      is_global = true;
    }
  } else {
    auto &var_info = lookup_var_info(var);
    if (var_info.type != result &&
        !(var_info.type == VarType::FLOAT && result == VarType::INT)) {
      ctx.add_error(
          ErrorHandler::ErrorLevel::TYPE,
          "类型不匹配: " + var + " 是 " + var_type_to_string(var_info.type) +
              " 类型，但赋值表达式是 " + var_type_to_string(result) + " 类型",
          line, __FILE__, __LINE__);
      return -1;
    }
  }
  return 0;
}

int Assign::gencode_stmt() {
  auto &var_info = lookup_var_info(var);
  VarType type = var_info.type;
  if (type == VarType::NONE) {
    throw std::runtime_error("未定义的变量: " + var +
                             " source:" + std::to_string(line) +
                             " file:" + std::string(__FILE__) +
                             " line:" + std::to_string(__LINE__));
  }

  // GenLocalVar(*assign);
  auto ptr = lookup_var_llvm_obj(var);
  if (!ptr) {
    throw std::runtime_error(
        "未定义的变量: " + var + " code:" + std::to_string(line) +
        " file:" + std::string(__FILE__) + " line:" + std::to_string(__LINE__));
  }
  auto llvm_value = value->gencode_expr(type);

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

llvm::Value *Assign::gencode_expr(VarType expected_type) {
  return value->gencode_expr(expected_type);
}