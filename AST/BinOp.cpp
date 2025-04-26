#include "BinOp.h"
#include "Common.h"
#include "TypeChecker.h"

int BinOp::visit_stmt() {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "二元运算不能作为语句使用", line, __FILE__, __LINE__);
    return -1;
}

int BinOp::handle_equality_op() {
  std::shared_ptr<ASTNode> left_ast;
  int ret = left->visit_expr(left_ast);
  if (ret == -1) {
    return -1;
  }
  
  std::shared_ptr<ASTNode> right_ast;
  ret = right->visit_expr(right_ast);
  if (ret == -1) {
    return -1;
  }
  
  if (left_ast->type != right_ast->type) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "无效类型用于 " + op + ": " + var_type_to_string(left_ast->type) + ", " +
                      var_type_to_string(right_ast->type),
                  line, __FILE__, __LINE__);
    return -1;
  }
  type = VarType::BOOL;
  return 0;
}

int BinOp::handle_comparison_op() {
  std::shared_ptr<ASTNode> left_ast;
  int ret = left->visit_expr(left_ast);
  if (ret == -1) {
    return -1;
  }
  
  std::shared_ptr<ASTNode> right_ast;
  ret = right->visit_expr(right_ast);
  if (ret == -1) {
    return -1;
  }
  
  if (left_ast->type != right_ast->type ||
      (left_ast->type != VarType::INT && left_ast->type != VarType::FLOAT)) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "无效类型用于 " + op + ": " + var_type_to_string(left_ast->type) + ", " + var_type_to_string(right_ast->type),
                  line, __FILE__, __LINE__);
    return -1;
  }
  type = VarType::BOOL;
  return 0;
}

int BinOp::handle_logical_op() {
  std::shared_ptr<ASTNode> left_ast;
  int ret = left->visit_expr(left_ast);
  if (ret == -1) {
    return -1;
  }
  
  std::shared_ptr<ASTNode> right_ast;
  ret = right->visit_expr(right_ast);
  if (ret == -1) {
    return -1;
  }
  
  if (left_ast->type != VarType::BOOL || right_ast->type != VarType::BOOL) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "无效类型用于 " + op + ": " + var_type_to_string(left_ast->type) + ", " + var_type_to_string(right_ast->type),
                  line, __FILE__, __LINE__);
    return -1;
  }
  type = VarType::BOOL;
  return 0;
}

int BinOp::handle_arithmetic_op() {
  std::shared_ptr<ASTNode> left_ast;
  int ret = left->visit_expr(left_ast);
  // std::cout << "binop visit_expr left: " << var_type_to_string(left_type) << " " << line << std::endl;
  if (ret == -1) {
    return -1;
  }
  
  std::shared_ptr<ASTNode> right_ast;
  ret = right->visit_expr(right_ast);
  // std::cout << "binop visit_expr right: " << var_type_to_string(right_type) << " " << line << std::endl;
  if (ret == -1) {
    return -1;
  }
  
  if (left_ast->type != right_ast->type &&
      !(left_ast->type == VarType::FLOAT && right_ast->type == VarType::INT) &&
      !(left_ast->type == VarType::INT && right_ast->type == VarType::FLOAT)) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "类型不匹配在 " + op + ": " + var_type_to_string(left_ast->type) + ", " + var_type_to_string(right_ast->type),
                  line, __FILE__, __LINE__);
    return -1;
  }
  if (left_ast->type != VarType::INT && left_ast->type != VarType::FLOAT && left_ast->type != VarType::STRING) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "无效类型用于 " + op + ": " + var_type_to_string(left_ast->type), line, __FILE__, __LINE__);
    return -1;
  }
  if (left_ast->type == VarType::FLOAT || right_ast->type == VarType::FLOAT ||
      op == "/") {
    type = VarType::FLOAT;
  } else {
    type = left_ast->type;
  }
  return 0;
}

int BinOp::handle_power_op() {
  std::shared_ptr<ASTNode> left_ast;
  int ret = left->visit_expr(left_ast);
  if (ret == -1) {
    return -1;
  }
  
  std::shared_ptr<ASTNode> right_ast;
  ret = right->visit_expr(right_ast);
  if (ret == -1) {
    return -1;
  }
  
  if (left_ast->type != right_ast->type &&
      !(left_ast->type == VarType::FLOAT && right_ast->type == VarType::INT) &&
      !(left_ast->type == VarType::INT && right_ast->type == VarType::FLOAT)) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "类型不匹配在 " + op + ": " + var_type_to_string(left_ast->type) + ", " + var_type_to_string(right_ast->type),
                  line, __FILE__, __LINE__);
    return -1;
  }
  if (left_ast->type != VarType::INT && left_ast->type != VarType::FLOAT) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "无效类型用于 " + op + ": " + var_type_to_string(left_ast->type), line, __FILE__, __LINE__);
    return -1;
  }
  if (left_ast->type == VarType::FLOAT || right_ast->type == VarType::FLOAT) {
    type = VarType::FLOAT;
  } else {
    type = left_ast->type;
  }
  return 0;
}

bool BinOp::is_string_concat(VarType left_type, VarType right_type) {
  return op == "+" && (left_type == VarType::STRING || right_type == VarType::STRING);
}

llvm::Value* BinOp::gen_add(VarType expected_type) {
  std::shared_ptr<ASTNode> left_ast;
  left->visit_expr(left_ast);
  std::shared_ptr<ASTNode> right_ast;
  right->visit_expr(right_ast);
  
  // 处理类型
  bool is_string_concat_op = is_string_concat(left_ast->type, right_ast->type);
  if (is_string_concat_op) {
    // 字符串连接操作
    auto left_value = left->gencode_expr(VarType::STRING);
    auto right_value = right->gencode_expr(VarType::STRING);
    
    // 如果左操作数不是字符串，需要将其转换为字符串
    if (left_ast->type != VarType::STRING) {
      throw std::runtime_error("不支持的操作: 非字符串 + 字符串 code:" +
                              std::to_string(line) +
                              " line:" + std::to_string(__LINE__));
    }

    // 如果右操作数不是字符串，需要将其转换为字符串
    if (right_ast->type != VarType::STRING) {
      throw std::runtime_error("不支持的操作: 字符串 + 非字符串 code:" +
                              std::to_string(line) +
                              " line:" + std::to_string(__LINE__));
    }

    // 两者都是字符串，调用运行时库进行连接
    auto concat_func = ctx.runtime_manager->getRuntimeFunction("concat_strings");
    if (!concat_func) {
      throw std::runtime_error(
          "无法获取字符串连接函数 code:" + std::to_string(line) +
          " line:" + std::to_string(__LINE__));
    }

    // 调用concat_strings函数连接字符串
    return ctx.builder->CreateCall(concat_func, {left_value, right_value}, "str_concat");
  } else {
    // 确定结果类型
    VarType result_type = (left_ast->type == VarType::FLOAT || right_ast->type == VarType::FLOAT) 
                          ? VarType::FLOAT : VarType::INT;
    
    // 生成操作数值
    auto left_value = left->gencode_expr(result_type);
    auto right_value = right->gencode_expr(result_type);
    
    // 执行加法运算
    llvm::Value* result;
    if (result_type == VarType::FLOAT) {
      result = ctx.builder->CreateFAdd(left_value, right_value, "fadd");
    } else {
      result = ctx.builder->CreateAdd(left_value, right_value, "add");
    }
    
    // 根据期望类型进行隐式转换
    if (expected_type != VarType::NONE && expected_type != result_type) {
      if (result_type == VarType::INT && expected_type == VarType::FLOAT) {
        return ctx.builder->CreateSIToFP(result, ctx.builder->getDoubleTy(), "int_to_float");
      } else if (result_type == VarType::FLOAT && expected_type == VarType::INT) {
        return ctx.builder->CreateFPToSI(result, ctx.builder->getInt64Ty(), "float_to_int");
      }
    }
    return result;
  }
}

llvm::Value* BinOp::gen_subtract(VarType expected_type) {
  std::shared_ptr<ASTNode> left_ast;
  left->visit_expr(left_ast);
  std::shared_ptr<ASTNode> right_ast;
  right->visit_expr(right_ast);
  
  // 确定结果类型
  VarType result_type = (left_ast->type == VarType::FLOAT || right_ast->type == VarType::FLOAT) 
                        ? VarType::FLOAT : VarType::INT;
  
  // 生成操作数值
  auto left_value = left->gencode_expr(result_type);
  auto right_value = right->gencode_expr(result_type);
  
  // 执行减法运算
  llvm::Value* result;
  if (result_type == VarType::FLOAT) {
    result = ctx.builder->CreateFSub(left_value, right_value, "fsub");
  } else {
    result = ctx.builder->CreateSub(left_value, right_value, "sub");
  }
  
  // 根据期望类型进行隐式转换
  if (expected_type != VarType::NONE && expected_type != result_type) {
    if (result_type == VarType::INT && expected_type == VarType::FLOAT) {
      return ctx.builder->CreateSIToFP(result, ctx.builder->getDoubleTy(), "int_to_float");
    } else if (result_type == VarType::FLOAT && expected_type == VarType::INT) {
      return ctx.builder->CreateFPToSI(result, ctx.builder->getInt64Ty(), "float_to_int");
    }
  }
  return result;
}

llvm::Value* BinOp::gen_multiply(VarType expected_type) {
  std::shared_ptr<ASTNode> left_ast;
  left->visit_expr(left_ast);
  std::shared_ptr<ASTNode> right_ast;
  right->visit_expr(right_ast);
  
  // 确定结果类型
  VarType result_type = (left_ast->type == VarType::FLOAT || right_ast->type == VarType::FLOAT) 
                        ? VarType::FLOAT : VarType::INT;
  
  // 生成操作数值
  auto left_value = left->gencode_expr(result_type);
  auto right_value = right->gencode_expr(result_type);
  
  // 执行乘法运算
  llvm::Value* result;
  if (result_type == VarType::FLOAT) {
    result = ctx.builder->CreateFMul(left_value, right_value, "fmul");
  } else {
    result = ctx.builder->CreateMul(left_value, right_value, "mul");
  }
  
  // 根据期望类型进行隐式转换
  if (expected_type != VarType::NONE && expected_type != result_type) {
    if (result_type == VarType::INT && expected_type == VarType::FLOAT) {
      return ctx.builder->CreateSIToFP(result, ctx.builder->getDoubleTy(), "int_to_float");
    } else if (result_type == VarType::FLOAT && expected_type == VarType::INT) {
      return ctx.builder->CreateFPToSI(result, ctx.builder->getInt64Ty(), "float_to_int");
    }
  }
  return result;
}

llvm::Value* BinOp::gen_divide(VarType expected_type) {
  // 除法总是产生浮点数结果
  VarType result_type = VarType::FLOAT;
  
  // 生成操作数值
  auto left_value = left->gencode_expr(result_type);
  auto right_value = right->gencode_expr(result_type);
  
  // 执行除法运算
  auto result = ctx.builder->CreateFDiv(left_value, right_value, "fdiv");
  
  // 根据期望类型进行隐式转换
  if (expected_type == VarType::INT) {
    return ctx.builder->CreateFPToSI(result, ctx.builder->getInt64Ty(), "float_to_int");
  }
  return result;
}

llvm::Value* BinOp::gen_floor_divide(VarType expected_type) {
  // 整数除法总是产生整数结果
  VarType result_type = VarType::INT;
  
  // 生成操作数值
  auto left_value = left->gencode_expr(result_type);
  auto right_value = right->gencode_expr(result_type);
  
  // 执行整数除法
  auto result = ctx.builder->CreateSDiv(left_value, right_value, "sdiv");
  
  // 根据期望类型进行隐式转换
  if (expected_type == VarType::FLOAT) {
    return ctx.builder->CreateSIToFP(result, ctx.builder->getDoubleTy(), "int_to_float");
  }
  return result;
}

llvm::Value* BinOp::gen_modulo(VarType expected_type) {
  std::shared_ptr<ASTNode> left_ast;
  left->visit_expr(left_ast);
  std::shared_ptr<ASTNode> right_ast;
  right->visit_expr(right_ast);
  
  // 确定结果类型
  VarType result_type = (left_ast->type == VarType::FLOAT || right_ast->type == VarType::FLOAT) 
                        ? VarType::FLOAT : VarType::INT;
  
  // 生成操作数值
  auto left_value = left->gencode_expr(result_type);
  auto right_value = right->gencode_expr(result_type);
  
  // 执行取模运算
  llvm::Value* result;
  if (result_type == VarType::FLOAT) {
    result = ctx.builder->CreateFRem(left_value, right_value, "frem");
  } else {
    result = ctx.builder->CreateSRem(left_value, right_value, "srem");
  }
  
  // 根据期望类型进行隐式转换
  if (expected_type != VarType::NONE && expected_type != result_type) {
    if (result_type == VarType::INT && expected_type == VarType::FLOAT) {
      return ctx.builder->CreateSIToFP(result, ctx.builder->getDoubleTy(), "int_to_float");
    } else if (result_type == VarType::FLOAT && expected_type == VarType::INT) {
      return ctx.builder->CreateFPToSI(result, ctx.builder->getInt64Ty(), "float_to_int");
    }
  }
  return result;
}

llvm::Value* BinOp::gen_power(VarType expected_type) {
  std::shared_ptr<ASTNode> left_ast;
  left->visit_expr(left_ast);
  std::shared_ptr<ASTNode> right_ast;
  right->visit_expr(right_ast);
  
  // 确定结果类型
  VarType result_type = (left_ast->type == VarType::FLOAT || right_ast->type == VarType::FLOAT) 
                        ? VarType::FLOAT : VarType::INT;
  
  // 获取pow函数
  auto pow_func = ctx.module->getOrInsertFunction("pow", 
    llvm::FunctionType::get(ctx.builder->getDoubleTy(), 
    {ctx.builder->getDoubleTy(), ctx.builder->getDoubleTy()}, false));
  
  // 生成操作数值并转换为浮点数(pow需要浮点数参数)
  llvm::Value* left_double;
  llvm::Value* right_double;
  
  if (left_ast->type == VarType::FLOAT) {
    left_double = left->gencode_expr(VarType::FLOAT);
  } else {
    auto left_value = left->gencode_expr(VarType::INT);
    left_double = ctx.builder->CreateSIToFP(left_value, ctx.builder->getDoubleTy());
  }
  
  if (right_ast->type == VarType::FLOAT) {
    right_double = right->gencode_expr(VarType::FLOAT);
  } else {
    auto right_value = right->gencode_expr(VarType::INT);
    right_double = ctx.builder->CreateSIToFP(right_value, ctx.builder->getDoubleTy());
  }
  
  // 调用pow函数
  auto pow_result = ctx.builder->CreateCall(pow_func, {left_double, right_double}, "pow");
  
  // 根据期望类型和结果类型进行转换
  if (result_type == VarType::INT || expected_type == VarType::INT) {
    return ctx.builder->CreateFPToSI(pow_result, ctx.builder->getInt64Ty(), "pow_int");
  }
  return pow_result;
}

llvm::Value* BinOp::gen_equal(VarType expected_type) {
  std::shared_ptr<ASTNode> left_ast;
  left->visit_expr(left_ast);
  std::shared_ptr<ASTNode> right_ast;
  right->visit_expr(right_ast);
  
  // 确定比较类型
  VarType compare_type = (left_ast->type == VarType::FLOAT || right_ast->type == VarType::FLOAT) 
                        ? VarType::FLOAT : left_ast->type;
  
  // 生成操作数值
  auto left_value = left->gencode_expr(compare_type);
  auto right_value = right->gencode_expr(compare_type);
  
  // 执行比较运算
  llvm::Value* result;
  if (compare_type == VarType::FLOAT) {
    result = ctx.builder->CreateFCmpOEQ(left_value, right_value, "feq");
  } else {
    result = ctx.builder->CreateICmpEQ(left_value, right_value, "eq");
  }
  
  // 根据期望类型进行隐式转换
  if (expected_type != VarType::NONE && expected_type != VarType::BOOL) {
    if (expected_type == VarType::INT) {
      return ctx.builder->CreateZExt(result, ctx.builder->getInt64Ty(), "bool_to_int");
    } else if (expected_type == VarType::FLOAT) {
      return ctx.builder->CreateUIToFP(result, ctx.builder->getDoubleTy(), "bool_to_float");
    }
  }
  return result;
}

llvm::Value* BinOp::gen_not_equal(VarType expected_type) {
  std::shared_ptr<ASTNode> left_ast;
  left->visit_expr(left_ast);
  std::shared_ptr<ASTNode> right_ast;
  right->visit_expr(right_ast);
  
  // 确定比较类型
  VarType compare_type = (left_ast->type == VarType::FLOAT || right_ast->type == VarType::FLOAT) 
                        ? VarType::FLOAT : left_ast->type;
  
  // 生成操作数值
  auto left_value = left->gencode_expr(compare_type);
  auto right_value = right->gencode_expr(compare_type);
  
  // 执行比较运算
  llvm::Value* result;
  if (compare_type == VarType::FLOAT) {
    result = ctx.builder->CreateFCmpONE(left_value, right_value, "fne");
  } else {
    result = ctx.builder->CreateICmpNE(left_value, right_value, "ne");
  }
  
  // 根据期望类型进行隐式转换
  if (expected_type != VarType::NONE && expected_type != VarType::BOOL) {
    if (expected_type == VarType::INT) {
      return ctx.builder->CreateZExt(result, ctx.builder->getInt64Ty(), "bool_to_int");
    } else if (expected_type == VarType::FLOAT) {
      return ctx.builder->CreateUIToFP(result, ctx.builder->getDoubleTy(), "bool_to_float");
    }
  }
  return result;
}

llvm::Value* BinOp::gen_less_than(VarType expected_type) {
  std::shared_ptr<ASTNode> left_ast;
  left->visit_expr(left_ast);
  std::shared_ptr<ASTNode> right_ast;
  right->visit_expr(right_ast);
  
  // 确定比较类型
  VarType compare_type;
  if (left_ast->type == VarType::FLOAT || right_ast->type == VarType::FLOAT) {
    compare_type = VarType::FLOAT;
  } else if (left_ast->type == VarType::BOOL || right_ast->type == VarType::BOOL) {
    compare_type = VarType::INT; // 布尔值转为INT进行比较
  } else {
    compare_type = left_ast->type;
  }
  
  // 生成操作数值
  llvm::Value* left_value;
  llvm::Value* right_value;
  
  if (left_ast->type == VarType::BOOL && compare_type == VarType::INT) {
    left_value = left->gencode_expr(VarType::BOOL);
    left_value = ctx.builder->CreateZExt(left_value, ctx.builder->getInt64Ty());
  } else {
    left_value = left->gencode_expr(compare_type);
  }
  
  if (right_ast->type == VarType::BOOL && compare_type == VarType::INT) {
    right_value = right->gencode_expr(VarType::BOOL);
    right_value = ctx.builder->CreateZExt(right_value, ctx.builder->getInt64Ty());
  } else {
    right_value = right->gencode_expr(compare_type);
  }
  
  // 执行比较运算
  llvm::Value* result;
  if (compare_type == VarType::FLOAT) {
    result = ctx.builder->CreateFCmpOLT(left_value, right_value, "fcmp");
  } else {
    result = ctx.builder->CreateICmpSLT(left_value, right_value, "cmp");
  }
  
  // 根据期望类型进行隐式转换
  if (expected_type != VarType::NONE && expected_type != VarType::BOOL) {
    if (expected_type == VarType::INT) {
      return ctx.builder->CreateZExt(result, ctx.builder->getInt64Ty(), "bool_to_int");
    } else if (expected_type == VarType::FLOAT) {
      return ctx.builder->CreateUIToFP(result, ctx.builder->getDoubleTy(), "bool_to_float");
    }
  }
  return result;
}

llvm::Value* BinOp::gen_greater_than(VarType expected_type) {
  VarType left_expected_type = VarType::NONE;
  VarType right_expected_type = VarType::NONE;
  
  std::shared_ptr<ASTNode> left_ast;
  left->visit_expr(left_ast);
  std::shared_ptr<ASTNode> right_ast;
  right->visit_expr(right_ast);
  
  // 生成操作数值
  auto left_value = left->gencode_expr(left_expected_type);
  auto right_value = right->gencode_expr(right_expected_type);
  
  // 处理布尔类型
  if (left_ast->type == VarType::BOOL) {
    left_value = ctx.builder->CreateZExt(left_value, ctx.builder->getInt64Ty());
  }
  if (right_ast->type == VarType::BOOL) {
    right_value = ctx.builder->CreateZExt(right_value, ctx.builder->getInt64Ty());
  }
  
  // 如果有一个是布尔值，将类型统一为INT进行比较
  if (left_ast->type == VarType::BOOL || right_ast->type == VarType::BOOL) {
    auto result = ctx.builder->CreateICmpSGT(left_value, right_value, "cmp");
    
    // 检查期望类型，如果需要转换为BOOL
    if (expected_type == VarType::BOOL) {
      return ctx.builder->CreateZExt(result, ctx.builder->getInt1Ty(), "bool_to_bool");
    }
    return result;
  }

  // 处理类型
  if (left_ast->type == VarType::FLOAT || right_ast->type == VarType::FLOAT) {
    // 浮点数比较
    if (left_ast->type == VarType::INT) {
      left_value = ctx.builder->CreateSIToFP(left_value, ctx.builder->getDoubleTy());
    }
    if (right_ast->type == VarType::INT) {
      right_value = ctx.builder->CreateSIToFP(right_value, ctx.builder->getDoubleTy());
    }
    
    auto result = ctx.builder->CreateFCmpOGT(left_value, right_value, "fcmp");
    
    // 检查期望类型，如果需要转换为BOOL
    if (expected_type == VarType::BOOL) {
      return ctx.builder->CreateZExt(result, ctx.builder->getInt1Ty(), "bool_to_bool");
    }
    return result;
  } else {
    // 整数比较
    auto result = ctx.builder->CreateICmpSGT(left_value, right_value, "cmp");
    
    // 检查期望类型，如果需要转换为BOOL
    if (expected_type == VarType::BOOL) {
      return ctx.builder->CreateZExt(result, ctx.builder->getInt1Ty(), "bool_to_bool");
    }
    return result;
  }
}

llvm::Value* BinOp::gen_greater_equal(VarType expected_type) {
  VarType left_expected_type = VarType::NONE;
  VarType right_expected_type = VarType::NONE;
  
  std::shared_ptr<ASTNode> left_ast;
  left->visit_expr(left_ast);
  std::shared_ptr<ASTNode> right_ast;
  right->visit_expr(right_ast);
  
  // 生成操作数值
  auto left_value = left->gencode_expr(left_expected_type);
  auto right_value = right->gencode_expr(right_expected_type);
  
  // 处理布尔类型
  if (left_ast->type == VarType::BOOL) {
    left_value = ctx.builder->CreateZExt(left_value, ctx.builder->getInt64Ty());
  }
  if (right_ast->type == VarType::BOOL) {
    right_value = ctx.builder->CreateZExt(right_value, ctx.builder->getInt64Ty());
  }
  
  // 如果有一个是布尔值，将类型统一为INT进行比较
  if (left_ast->type == VarType::BOOL || right_ast->type == VarType::BOOL) {
    auto result = ctx.builder->CreateICmpSGE(left_value, right_value, "cmp");
    
    // 检查期望类型，如果需要转换为BOOL
    if (expected_type == VarType::BOOL) {
      return ctx.builder->CreateZExt(result, ctx.builder->getInt1Ty(), "bool_to_bool");
    }
    return result;
  }
  
  // 处理类型
  if (left_ast->type == VarType::FLOAT || right_ast->type == VarType::FLOAT) {
    // 浮点数比较
    if (left_ast->type == VarType::INT) {
      left_value = ctx.builder->CreateSIToFP(left_value, ctx.builder->getDoubleTy());
    }
    if (right_ast->type == VarType::INT) {
      right_value = ctx.builder->CreateSIToFP(right_value, ctx.builder->getDoubleTy());
    }
    
    auto result = ctx.builder->CreateFCmpOGE(left_value, right_value, "fcmp");
    
    // 检查期望类型，如果需要转换为BOOL
    if (expected_type == VarType::BOOL) {
      return ctx.builder->CreateZExt(result, ctx.builder->getInt1Ty(), "bool_to_bool");
    }
    return result;
  } else {
    // 整数比较
    auto result = ctx.builder->CreateICmpSGE(left_value, right_value, "cmp");
    
    // 检查期望类型，如果需要转换为BOOL
    if (expected_type == VarType::BOOL) {
      return ctx.builder->CreateZExt(result, ctx.builder->getInt1Ty(), "bool_to_bool");
    }
    return result;
  }
}

llvm::Value* BinOp::gen_less_equal(VarType expected_type) {
  VarType left_expected_type = VarType::NONE;
  VarType right_expected_type = VarType::NONE;
  
  std::shared_ptr<ASTNode> left_ast;
  left->visit_expr(left_ast);
  std::shared_ptr<ASTNode> right_ast;
  right->visit_expr(right_ast);
  
  // 生成操作数值
  auto left_value = left->gencode_expr(left_expected_type);
  auto right_value = right->gencode_expr(right_expected_type);
  
  // 处理布尔类型
  if (left_ast->type == VarType::BOOL) {
    left_value = ctx.builder->CreateZExt(left_value, ctx.builder->getInt64Ty());
  }
  if (right_ast->type == VarType::BOOL) {
    right_value = ctx.builder->CreateZExt(right_value, ctx.builder->getInt64Ty());
  }
  
  // 如果有一个是布尔值，将类型统一为INT进行比较
  if (left_ast->type == VarType::BOOL || right_ast->type == VarType::BOOL) {
    auto result = ctx.builder->CreateICmpSLE(left_value, right_value, "cmp");
    
    // 检查期望类型，如果需要转换为BOOL
    if (expected_type == VarType::BOOL) {
      return ctx.builder->CreateZExt(result, ctx.builder->getInt1Ty(), "bool_to_bool");
    }
    return result;
  }
  
  // 处理类型
  if (left_ast->type == VarType::FLOAT || right_ast->type == VarType::FLOAT) {
    // 浮点数比较
    if (left_ast->type == VarType::INT) {
      left_value = ctx.builder->CreateSIToFP(left_value, ctx.builder->getDoubleTy());
    }
    if (right_ast->type == VarType::INT) {
      right_value = ctx.builder->CreateSIToFP(right_value, ctx.builder->getDoubleTy());
    }
    
    auto result = ctx.builder->CreateFCmpOLE(left_value, right_value, "fcmp");
    
    // 检查期望类型，如果需要转换为BOOL
    if (expected_type == VarType::BOOL) {
      return ctx.builder->CreateZExt(result, ctx.builder->getInt1Ty(), "bool_to_bool");
    }
    return result;
  } else {
    // 整数比较
    auto result = ctx.builder->CreateICmpSLE(left_value, right_value, "cmp");
    
    // 检查期望类型，如果需要转换为BOOL
    if (expected_type == VarType::BOOL) {
      return ctx.builder->CreateZExt(result, ctx.builder->getInt1Ty(), "bool_to_bool");
    }
    return result;
  }
}

llvm::Value* BinOp::gen_logical_and(VarType expected_type) {
  // 生成短路逻辑
  auto function = ctx.builder->GetInsertBlock()->getParent();
  auto entry_bb = ctx.builder->GetInsertBlock();
  
  // 创建基本块
  auto right_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "and.right", function);
  auto merge_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "and.end");
  
  // 生成左操作数
  auto left_value = left->gencode_expr(VarType::BOOL);
  
  // 条件跳转 - 如果左边为false，短路并返回false
  ctx.builder->CreateCondBr(left_value, right_bb, merge_bb);
  
  // 右操作数代码块
  ctx.builder->SetInsertPoint(right_bb);
  auto right_value = right->gencode_expr(VarType::BOOL);
  auto right_bb_end = ctx.builder->GetInsertBlock(); // 获取当前右侧代码块，可能已经改变
  ctx.builder->CreateBr(merge_bb);
  
  // 合并块
  function->getBasicBlockList().push_back(merge_bb);
  ctx.builder->SetInsertPoint(merge_bb);
  
  // 创建PHI节点来合并结果
  auto phi = ctx.builder->CreatePHI(ctx.builder->getInt1Ty(), 2, "and.result");
  phi->addIncoming(ctx.builder->getFalse(), entry_bb);
  phi->addIncoming(right_value, right_bb_end);
  
  // 根据期望类型进行隐式转换
  if (expected_type != VarType::NONE && expected_type != VarType::BOOL) {
    if (expected_type == VarType::INT) {
      return ctx.builder->CreateZExt(phi, ctx.builder->getInt64Ty(), "bool_to_int");
    } else if (expected_type == VarType::FLOAT) {
      return ctx.builder->CreateUIToFP(phi, ctx.builder->getDoubleTy(), "bool_to_float");
    }
  }
  return phi;
}

llvm::Value* BinOp::gen_logical_or(VarType expected_type) {
  // 生成短路逻辑
  auto function = ctx.builder->GetInsertBlock()->getParent();
  auto entry_bb = ctx.builder->GetInsertBlock();
  
  // 创建基本块
  auto right_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "or.right", function);
  auto merge_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "or.end");
  
  // 生成左操作数
  auto left_value = left->gencode_expr(VarType::BOOL);
  
  // 条件跳转 - 如果左边为true，短路并返回true
  ctx.builder->CreateCondBr(left_value, merge_bb, right_bb);
  
  // 右操作数代码块
  ctx.builder->SetInsertPoint(right_bb);
  auto right_value = right->gencode_expr(VarType::BOOL);
  auto right_bb_end = ctx.builder->GetInsertBlock(); // 获取当前右侧代码块，可能已经改变
  ctx.builder->CreateBr(merge_bb);
  
  // 合并块
  function->getBasicBlockList().push_back(merge_bb);
  ctx.builder->SetInsertPoint(merge_bb);
  
  // 创建PHI节点来合并结果
  auto phi = ctx.builder->CreatePHI(ctx.builder->getInt1Ty(), 2, "or.result");
  phi->addIncoming(ctx.builder->getTrue(), entry_bb);
  phi->addIncoming(right_value, right_bb_end);
  
  // 根据期望类型进行隐式转换
  if (expected_type != VarType::NONE && expected_type != VarType::BOOL) {
    if (expected_type == VarType::INT) {
      return ctx.builder->CreateZExt(phi, ctx.builder->getInt64Ty(), "bool_to_int");
    } else if (expected_type == VarType::FLOAT) {
      return ctx.builder->CreateUIToFP(phi, ctx.builder->getDoubleTy(), "bool_to_float");
    }
  }
  return phi;
}

int BinOp::visit_expr(std::shared_ptr<ASTNode> &self) {
  // 分发到不同的运算符处理函数
  self = shared_from_this();
  if (op == "==" || op == "!=") {
    return handle_equality_op();
  }
  if (op == "<" || op == ">" || op == ">=" || op == "<=") {
    return handle_comparison_op();
  }
  if (op == "and" || op == "or") {
    return handle_logical_op();
  }
  if (op == "+" || op == "-" || op == "*" || op == "/" || op == "//" || op == "%") {
    return handle_arithmetic_op();
  }
  if (op == "**") {
    return handle_power_op();
  }
  ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未知运算符: " + op, line, __FILE__, __LINE__);
  return -1;
}

int BinOp::gencode_stmt() { return 0; }

// 更新gencode_expr函数，传递expected_type给各操作符处理函数
llvm::Value *BinOp::gencode_expr(VarType expected_type) {
  // 根据运算符分发到相应的代码生成函数
  if (op == "+") {
    return gen_add(expected_type);
  }
  if (op == "-") {
    return gen_subtract(expected_type);
  }
  if (op == "*") {
    return gen_multiply(expected_type);
  }
  if (op == "/") {
    return gen_divide(expected_type);
  }
  if (op == "//") {
    return gen_floor_divide(expected_type);
  }
  if (op == "%") {
    return gen_modulo(expected_type);
  }
  if (op == "**") {
    return gen_power(expected_type);
  }
  if (op == "==") {
    return gen_equal(expected_type);
  }
  if (op == "!=") {
    return gen_not_equal(expected_type);
  }
  if (op == "<") {
    return gen_less_than(expected_type);
  }
  if (op == ">") {
    return gen_greater_than(expected_type);
  }
  if (op == ">=") {
    return gen_greater_equal(expected_type);
  }
  if (op == "<=") {
    return gen_less_equal(expected_type);
  }
  if (op == "and") {
    return gen_logical_and(expected_type);
  }
  if (op == "or") {
    return gen_logical_or(expected_type);
  }
  throw std::runtime_error("未知运算符: " + op +
                           " code:" + std::to_string(line) +
                           " line:" + std::to_string(__LINE__));
  return nullptr;
}