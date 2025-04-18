#include "BoolLiteral.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int BoolLiteral::visit_stmt(VarType &result) {
  ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "布尔字面量不能作为语句使用",
                line, __FILE__, __LINE__);
  return -1;
}

int BoolLiteral::visit_expr(VarType &result) {
  result = VarType::BOOL;
  return 0;
}