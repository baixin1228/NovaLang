#include "BoolLiteral.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int BoolLiteral::visit_stmt() {
  ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "布尔字面量不能作为语句使用",
                line, __FILE__, __LINE__);
  return -1;
}

int BoolLiteral::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
  type = VarType::BOOL;
  expr_ret = shared_from_this();
  return 0;
}

int BoolLiteral::gencode_stmt() { return 0; }

int BoolLiteral::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
  ret_value = ctx.builder->getInt1(value);
  // TODO: Handle expected_type conversion if necessary (e.g., bool to int/float)
  return 0; // Return 0 on success
}