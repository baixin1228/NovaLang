#include "BinOp.h"
#include "TypeChecker.h"

int BinOp::visit_stmt(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "二元运算不能作为语句使用", line, __FILE__, __LINE__);
    return -1;
}

int BinOp::visit_expr(VarType &result) {
  VarType left_type;
  VarType right_type;
  int ret = left->visit_expr(left_type);
  if (ret == -1) {
    return -1;
  }
  ret = right->visit_expr(right_type);
  if (ret == -1) {
    return -1;
  }

  if (op == "==") {
    if (left_type != right_type) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "无效类型用于 ==: " + var_type_to_string(left_type) + ", " +
                        var_type_to_string(right_type),
                    line, __FILE__, __LINE__);
      return -1;
    }
    result = VarType::BOOL;
    return 0;
  }
  if (op == "<" || op == ">" || op == ">=") {
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
  if (op == "+" || op == "-" || op == "*" || op == "/" || op == "//") {
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
  ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未知运算符: " + op, line, __FILE__, __LINE__);
  return -1;
}