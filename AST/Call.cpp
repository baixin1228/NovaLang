#include "Call.h"
#include "Context.h"
#include "Function.h"

int Call::visit_stmt(VarType &result) {
    return visit_expr(result);
}

int Call::visit_expr(VarType &result) {
  if (!ctx.has_func(name)) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义函数: " + name, line, __FILE__, __LINE__);
    return -1;
  }
  auto &[param_types, return_type] = ctx.get_func_type(name);
  if (args.size() != param_types.size()) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "参数数量不匹配: 期望 " + std::to_string(param_types.size()) +
                      ", 得到 " + std::to_string(args.size()),
                  line, __FILE__, __LINE__);
    return -1;
  }

  // update params type
  for (size_t i = 0; i < args.size(); i++) {
    VarType arg_type;
    int ret = args[i]->visit_expr(arg_type);
    if (ret == -1) {
      return -1;
    }
    if (param_types[i] != VarType::NONE && param_types[i] != arg_type) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "参数类型不匹配: 期望 " +
                        var_type_to_string(param_types[i]) + ", 得到 " +
                        var_type_to_string(arg_type),
                    line, __FILE__, __LINE__);
      return -1;
    }
    param_types[i] = arg_type;
  }

  const std::shared_ptr<ASTNode> &node = ctx.get_func(name);
  if (!node) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义函数: " + name, line, __FILE__, __LINE__);
    return -1;
  }
  if (auto func = dynamic_cast<Function *>(node.get())) {
    func->reference_count++;
    func->visit_stmt(result); // 正式推断类型，生成代码
  }
  std::tie(param_types, return_type) = ctx.get_func_type(name);

  result = return_type;
  return 0;
}