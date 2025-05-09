#include "IntLiteral.h"
#include "Context.h"

int IntLiteral::visit_stmt() {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "整数字面量不能作为语句使用", line, __FILE__, __LINE__);
    return -1;
}

int IntLiteral::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
  type = VarType::INT;
  expr_ret = shared_from_this();
  return 0;
} 

int IntLiteral::gencode_stmt() {
    return 0;
}

int IntLiteral::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
  if (expected_type != VarType::NONE && expected_type == VarType::FLOAT) {
    // 隐式类型转换
    ret_value = ctx.builder->CreateSIToFP(ctx.builder->getInt64(value),
                                ctx.builder->getDoubleTy());
  } else {
    ret_value = ctx.builder->getInt64(value);
  }
  return 0;
}