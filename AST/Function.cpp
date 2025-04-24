#include "Function.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int Function::visit_stmt(VarType &result) {
  auto &func_info = ctx.get_func_info(name);
  if (func_info.need_conjecture) {
    VarType return_type = VarType::NONE;
    if (func_info.param_types.size() != params.size()) {
      throw std::runtime_error(
          "function parameter number mismatch: " + name +
          " expected: " + std::to_string(func_info.param_types.size()) +
          " got: " + std::to_string(params.size()) + " file: " + __FILE__ +
          " line: " + std::to_string(__LINE__));
    }

    // set param type
    for (size_t i = 0; i < params.size(); i++) {
      add_var_info(params[i], true);
      auto& var_info = lookup_var_info(params[i]);
      var_info.type = func_info.param_types[i];
    }

    // process function body
    for (auto &stmt : body) {
      VarType stmt_type;
      int ret = stmt->visit_stmt(stmt_type);
      if (ret == -1) {
        return -1;
      }
      if (stmt_type != VarType::NONE) {
        return_type = stmt_type; // last return decide return type
      }
    }

    // verify and update return type
    if (func_info.return_type != VarType::NONE &&
        return_type != func_info.return_type) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "函数返回类型不匹配，期望: " +
                        var_type_to_string(func_info.return_type) +
                        "，得到: " + var_type_to_string(return_type),
                    line, __FILE__, __LINE__);
      return -1;
    } else if (return_type != VarType::NONE) {
      // update function return type
      func_info.return_type = return_type;
    }

    result = return_type;
    return 0;
  } else {
    for (auto &param : params) {
      /* add param type to func_info, type is none */
      func_info.param_types.push_back(VarType::NONE);
    }

    VarType return_type = VarType::NONE;
    func_info.return_type = return_type;
    func_info.need_conjecture = true;
    result = return_type;
    return 0;
  }
}

int Function::visit_expr(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "函数定义不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
}

int Function::gencode_stmt() { return 0; }

llvm::Value *Function::gencode_expr(VarType expected_type) {
    return nullptr;
}