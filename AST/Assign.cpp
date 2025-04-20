#include "Assign.h"
#include "ASTParser.h"
#include "TypeChecker.h"

int Assign::visit_stmt(VarType &result) {
  VarType type;
  int ret = value->visit_expr(type);
  if (ret == -1) {
    return -1;
  }

  if (add_var_type(var, type) != 0) {
    VarType existing_type = lookup_var_type(var);
    if (existing_type != type &&
        !(existing_type == VarType::FLOAT && type == VarType::INT)) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "变量：" + var +
                        " 类型不匹配，得到的类型: " + var_type_to_string(type) +
                        " 期望类型是：" + var_type_to_string(existing_type),
                    line, __FILE__, __LINE__);
      return -1;
    }
  } else {
    need_create = true;
    if (get_scope_depth() == 0) {
      is_global = true;
    }
  }
  if (type == VarType::STRUCT) {
    ctx.add_global_struct(var, value);
  }
  result = type;
  return 0;
}

int Assign::visit_expr(VarType &result) {
  int ret = value->visit_expr(result);
  if (ret == -1) {
    return -1;
  }

  if (add_var_type(var, result) != 0) {
    VarType left_type = lookup_var_type(var);
    if (left_type != result && !(left_type == VarType::FLOAT && result == VarType::INT)) {
      ctx.add_error(
          ErrorHandler::ErrorLevel::TYPE,
          "类型不匹配: " + var + " 是 " + var_type_to_string(left_type) +
              " 类型，但赋值表达式是 " + var_type_to_string(result) + " 类型",
          line, __FILE__, __LINE__);
      return -1;
    }
  } else {
    need_create = true;
    if (get_scope_depth() == 0) {
      is_global = true;
    }
  }
  return 0;
}