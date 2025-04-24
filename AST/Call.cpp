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
  auto &func_info = ctx.get_func_info(name);
  if (args.size() != func_info.param_types.size()) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "参数数量不匹配: 期望 " + std::to_string(func_info.param_types.size()) +
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
    if (func_info.param_types[i] != VarType::NONE && func_info.param_types[i] != arg_type) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "参数类型不匹配: 期望 " +
                        var_type_to_string(func_info.param_types[i]) + ", 得到 " +
                        var_type_to_string(arg_type),
                    line, __FILE__, __LINE__);
      return -1;
    }
    func_info.param_types[i] = arg_type;
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
  result = func_info.return_type;
  return 0;
}

int Call::gencode_stmt() {
  gencode_expr(VarType::NONE);
  return 0;
}

llvm::Value *Call::gencode_expr(VarType expected_type) {
  auto func = lookup_func_info(name);
  std::vector<llvm::Value *> llvm_args;
  for (auto &arg : args) {
    llvm_args.push_back(arg->gencode_expr(VarType::NONE));
  }
  return ctx.builder->CreateCall(func.llvm_obj, llvm_args, "call");
}