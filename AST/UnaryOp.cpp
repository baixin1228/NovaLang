#include "UnaryOp.h"
#include "Context.h"

int UnaryOp::visit_stmt() {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "一元运算不能作为语句使用", line, __FILE__, __LINE__);
    return -1;
}

int UnaryOp::visit_expr(std::shared_ptr<ASTNode> &self) {
    std::shared_ptr<ASTNode> operand_ast;
    int ret = expr->visit_expr(operand_ast);
    if (ret == -1) {
        return -1;
    }
    if (op == "not") {
        if (operand_ast->type != VarType::BOOL) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                             "not 运算符需要 bool 类型，得到: " + var_type_to_string(operand_ast->type), line, __FILE__, __LINE__);
        }
        type = VarType::BOOL;
        return 0;
    }
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未知一元运算符: " + op, line, __FILE__, __LINE__);
    return -1;
} 

int UnaryOp::gencode_stmt() { return 0; }

int UnaryOp::gencode_expr(VarType expected_type, llvm::Value *&value) {
  llvm::Value *expr_val = nullptr;
  if (expr->gencode_expr(expected_type, expr_val) != 0) {
    return -1;
  }
  
  if (op == "not") {
    value = ctx.builder->CreateNot(expr_val, "not");
    return 0;
  }
  
  ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未知一元运算符: " + op,
                line, __FILE__, __LINE__);
  return -1;
}