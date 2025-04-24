#include "BinOp.h"
#include "TypeChecker.h"

int BinOp::visit_stmt(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "二元运算不能作为语句使用", line, __FILE__, __LINE__);
    return -1;
}

int BinOp::visit_expr(VarType &result) {
  VarType left_type;
  VarType right_type;
  VarType left_ori_type;
  VarType right_ori_type;
  VarType left_expected_type = VarType::NONE;
  VarType right_expected_type = VarType::NONE;

  left->visit_expr(left_type);
  right->visit_expr(right_type);
  left_ori_type = left_type;
  right_ori_type = right_type;

  if (op == "==" || op == "!=") {
    if (left_type != right_type) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "无效类型用于 " + op + ": " + var_type_to_string(left_type) + ", " +
                        var_type_to_string(right_type),
                    line, __FILE__, __LINE__);
      return -1;
    }
    result = VarType::BOOL;
    return 0;
  }
  if (op == "<" || op == ">" || op == ">=" || op == "<=") {
    if (left_type != right_type ||
        (left_type != VarType::INT && left_type != VarType::FLOAT)) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "无效类型用于 " + op + ": " + var_type_to_string(left_type) + ", " + var_type_to_string(right_type),
                    line, __FILE__, __LINE__);
      return -1;
    }
    result = VarType::BOOL;
    return 0;
  }
  if (op == "and" || op == "or") {
    if (left_type != VarType::BOOL || right_type != VarType::BOOL) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "无效类型用于 " + op + ": " + var_type_to_string(left_type) + ", " + var_type_to_string(right_type),
                    line, __FILE__, __LINE__);
      return -1;
    }
    result = VarType::BOOL;
    return 0;
  }
  if (op == "+" || op == "-" || op == "*" || op == "/" || op == "//" || op == "%") {
    if (left_type != right_type &&
        !(left_type == VarType::FLOAT && right_type == VarType::INT) &&
        !(left_type == VarType::INT && right_type == VarType::FLOAT)) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "类型不匹配在 " + op + ": " + var_type_to_string(left_type) + ", " + var_type_to_string(right_type),
                    line, __FILE__, __LINE__);
      return -1;
    }
    if (left_type != VarType::INT && left_type != VarType::FLOAT && left_type != VarType::STRING) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "无效类型用于 " + op + ": " + var_type_to_string(left_type), line, __FILE__, __LINE__);
      return -1;
    }
    if (left_type == VarType::FLOAT || right_type == VarType::FLOAT ||
        op == "/") {
      result = VarType::FLOAT;
    } else {
      result = left_type;
    }
    return 0;
  }
  if (op == "**") {
    if (left_type != right_type &&
        !(left_type == VarType::FLOAT && right_type == VarType::INT) &&
        !(left_type == VarType::INT && right_type == VarType::FLOAT)) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "类型不匹配在 " + op + ": " + var_type_to_string(left_type) + ", " + var_type_to_string(right_type),
                    line, __FILE__, __LINE__);
      return -1;
    }
    if (left_type != VarType::INT && left_type != VarType::FLOAT) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "无效类型用于 " + op + ": " + var_type_to_string(left_type), line, __FILE__, __LINE__);
      return -1;
    }
    if (left_type == VarType::FLOAT || right_type == VarType::FLOAT) {
      result = VarType::FLOAT;
    } else {
      result = left_type;
    }
    return 0;
  }
  ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未知运算符: " + op, line, __FILE__, __LINE__);
  return -1;
}

int BinOp::gencode_stmt() { return 0; }

llvm::Value *BinOp::gencode_expr(VarType expected_type) {
  VarType left_type;
  VarType right_type;
  VarType left_ori_type;
  VarType right_ori_type;
  VarType left_expected_type = VarType::NONE;
  VarType right_expected_type = VarType::NONE;

  left->visit_expr(left_type);
  right->visit_expr(right_type);
  left_ori_type = left_type;
  right_ori_type = right_type;

  // 检查是否是字符串连接操作
  bool is_string_concat = op == "+" && (left_type == VarType::STRING ||
                                               right_type == VarType::STRING);

  // 如果不是字符串连接，按原逻辑处理类型转换
  if (!is_string_concat) {
    if (expected_type != VarType::NONE) {
      left_expected_type = expected_type;
      right_expected_type = expected_type;
      left_ori_type = expected_type;
    }
    if (left_type != right_type) {
      if (left_type == VarType::FLOAT) {
        right_expected_type = VarType::FLOAT;
      }
      if (right_type == VarType::FLOAT) {
        left_expected_type = VarType::FLOAT;
      }
      left_ori_type = VarType::FLOAT;
    }
    if (op == "/") {
      left_expected_type = VarType::FLOAT;
      right_expected_type = VarType::FLOAT;
      left_ori_type = VarType::FLOAT;
    }
  } else {
    // 对于字符串连接，设置返回类型为STRING
    left_ori_type = VarType::STRING;
  }

  auto left_value = left->gencode_expr(left_expected_type);
  auto right_value = right->gencode_expr(right_expected_type);

  if (left_value == nullptr || right_value == nullptr) {
    throw std::runtime_error("未知表达式 code:" + std::to_string(line) +
                             " line:" + std::to_string(__LINE__));
    return nullptr;
  }

  if (op == "+") {
    // 处理字符串连接
    if (is_string_concat) {
      // 如果左操作数不是字符串，需要将其转换为字符串
      if (left_type != VarType::STRING) {
        // 创建一个临时字符串值
        // 这里需要根据实际情况实现将不同类型转换为字符串的逻辑
        // 目前简单处理：只支持字符串+字符串
        throw std::runtime_error("不支持的操作: 非字符串 + 字符串 code:" +
                                  std::to_string(line) +
                                 " line:" + std::to_string(__LINE__));
      }

      // 如果右操作数不是字符串，需要将其转换为字符串
      if (right_type != VarType::STRING) {
        // 同上，需要实现类型转换
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
    }

    // 非字符串加法处理
    return left_ori_type == VarType::INT
               ? ctx.builder->CreateAdd(left_value, right_value, "add")
               : ctx.builder->CreateFAdd(left_value, right_value, "fadd");
  }
  if (op == "-") {
    return left_ori_type == VarType::INT
               ? ctx.builder->CreateSub(left_value, right_value, "sub")
               : ctx.builder->CreateFSub(left_value, right_value, "fsub");
  }
  if (op == "*") {
    return left_ori_type == VarType::INT
               ? ctx.builder->CreateMul(left_value, right_value, "mul")
               : ctx.builder->CreateFMul(left_value, right_value, "fmul");
  }
  if (op == "/") {
    return ctx.builder->CreateFDiv(left_value, right_value, "fdiv");
  }
  if (op == "//") {
    return ctx.builder->CreateSDiv(left_value, right_value, "sdiv");
  }
  if (op == "%") {
    return left_ori_type == VarType::INT
               ? ctx.builder->CreateSRem(left_value, right_value, "srem")
               : ctx.builder->CreateFRem(left_value, right_value, "frem");
  }
  if (op == "**") {
    // For exponentiation, we need to call a math function
    if (left_ori_type == VarType::FLOAT) {
      auto pow_func = ctx.module->getOrInsertFunction("pow", 
        llvm::FunctionType::get(ctx.builder->getDoubleTy(), 
        {ctx.builder->getDoubleTy(), ctx.builder->getDoubleTy()}, false));
      return ctx.builder->CreateCall(pow_func, {left_value, right_value}, "pow");
    } else {
      // For integer exponentiation, we'll need a custom implementation or loop
      // For simplicity, cast to double and use pow
      auto left_double = ctx.builder->CreateSIToFP(left_value, ctx.builder->getDoubleTy());
      auto right_double = ctx.builder->CreateSIToFP(right_value, ctx.builder->getDoubleTy());
      auto pow_func = ctx.module->getOrInsertFunction("pow", 
        llvm::FunctionType::get(ctx.builder->getDoubleTy(), 
        {ctx.builder->getDoubleTy(), ctx.builder->getDoubleTy()}, false));
      auto result_double = ctx.builder->CreateCall(pow_func, {left_double, right_double}, "pow");
      return ctx.builder->CreateFPToSI(result_double, ctx.builder->getInt64Ty(), "pow_int");
    }
  }
  if (op == "==") {
    if (left_ori_type == VarType::INT) {
      return ctx.builder->CreateICmpEQ(left_value, right_value, "eq");
    } else if (left_ori_type == VarType::FLOAT) {
      return ctx.builder->CreateFCmpOEQ(left_value, right_value, "feq");
    } else {
      return ctx.builder->CreateICmpEQ(left_value, right_value, "eq");
    }
  }
  if (op == "!=") {
    if (left_ori_type == VarType::INT) {
      return ctx.builder->CreateICmpNE(left_value, right_value, "ne");
    } else if (left_ori_type == VarType::FLOAT) {
      return ctx.builder->CreateFCmpONE(left_value, right_value, "fne");
    } else {
      return ctx.builder->CreateICmpNE(left_value, right_value, "ne");
    }
  }
  if (op == "<") {
    // 处理布尔类型
    if (left_ori_type == VarType::BOOL) {
      left_value = ctx.builder->CreateZExt(left_value, ctx.builder->getInt64Ty());
    }
    if (right_ori_type == VarType::BOOL) {
      right_value = ctx.builder->CreateZExt(right_value, ctx.builder->getInt64Ty());
    }
    
    // 如果有一个是布尔值，将类型统一为INT进行比较
    if (left_ori_type == VarType::BOOL || right_ori_type == VarType::BOOL) {
      return ctx.builder->CreateICmpSLT(left_value, right_value, "cmp");
    }
    
    return left_ori_type == VarType::INT
               ? ctx.builder->CreateICmpSLT(left_value, right_value, "cmp")
               : ctx.builder->CreateFCmpOLT(left_value, right_value, "fcmp");
  }
  if (op == ">") {
    // 处理布尔类型
    if (left_ori_type == VarType::BOOL) {
      left_value = ctx.builder->CreateZExt(left_value, ctx.builder->getInt64Ty());
    }
    if (right_ori_type == VarType::BOOL) {
      right_value = ctx.builder->CreateZExt(right_value, ctx.builder->getInt64Ty());
    }
    
    // 如果有一个是布尔值，将类型统一为INT进行比较
    if (left_ori_type == VarType::BOOL || right_ori_type == VarType::BOOL) {
      return ctx.builder->CreateICmpSGT(left_value, right_value, "cmp");
    }
    
    return left_ori_type == VarType::INT
               ? ctx.builder->CreateICmpSGT(left_value, right_value, "cmp")
               : ctx.builder->CreateFCmpOGT(left_value, right_value, "fcmp");
  }
  if (op == ">=") {
    // 处理布尔类型
    if (left_ori_type == VarType::BOOL) {
      left_value = ctx.builder->CreateZExt(left_value, ctx.builder->getInt64Ty());
    }
    if (right_ori_type == VarType::BOOL) {
      right_value = ctx.builder->CreateZExt(right_value, ctx.builder->getInt64Ty());
    }
    
    // 如果有一个是布尔值，将类型统一为INT进行比较
    if (left_ori_type == VarType::BOOL || right_ori_type == VarType::BOOL) {
      return ctx.builder->CreateICmpSGE(left_value, right_value, "cmp");
    }
    
    return left_ori_type == VarType::INT
               ? ctx.builder->CreateICmpSGE(left_value, right_value, "cmp")
               : ctx.builder->CreateFCmpOGE(left_value, right_value, "fcmp");
  }
  if (op == "<=") {
    // 处理布尔类型
    if (left_ori_type == VarType::BOOL) {
      left_value = ctx.builder->CreateZExt(left_value, ctx.builder->getInt64Ty());
    }
    if (right_ori_type == VarType::BOOL) {
      right_value = ctx.builder->CreateZExt(right_value, ctx.builder->getInt64Ty());
    }
    
    // 如果有一个是布尔值，将类型统一为INT进行比较
    if (left_ori_type == VarType::BOOL || right_ori_type == VarType::BOOL) {
      return ctx.builder->CreateICmpSLE(left_value, right_value, "cmp");
    }
    
    return left_ori_type == VarType::INT
               ? ctx.builder->CreateICmpSLE(left_value, right_value, "cmp")
               : ctx.builder->CreateFCmpOLE(left_value, right_value, "fcmp");
  }
  if (op == "and") {
    return ctx.builder->CreateAnd(left_value, right_value, "and");
  }
  if (op == "or") {
    return ctx.builder->CreateOr(left_value, right_value, "or");
  }
  throw std::runtime_error("未知运算符: " + op +
                           " code:" + std::to_string(line) +
                           " line:" + std::to_string(__LINE__));
  return nullptr;
}