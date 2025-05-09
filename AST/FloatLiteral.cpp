#include "FloatLiteral.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int FloatLiteral::visit_stmt() {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "浮点数字面量不能作为语句使用", line, __FILE__, __LINE__);
    return -1;
}

int FloatLiteral::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
    type = VarType::FLOAT;
    expr_ret = shared_from_this();
    return 0;
}

int FloatLiteral::gencode_stmt() { return 0; }

int FloatLiteral::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
    ret_value = llvm::ConstantFP::get(ctx.builder->getDoubleTy(), value);
    // TODO: Handle expected_type conversion if necessary (e.g., float to int)
    return 0; // Return 0 on success
}