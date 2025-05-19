#include "Void.h"
#include "TypeChecker.h"
#include "ASTParser.h"

Void::Void(Context &ctx, int line) : ASTNode(ctx, line) {
    type = VarType::VOID;
}

void Void::print(int level) {
    std::string indent(level * 2, ' ');
    std::cout << indent << "Void" << std::endl;
}

int Void::visit_stmt() {
    return 0;
}

int Void::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
    expr_ret = shared_from_this();
    return 0;
}

int Void::gencode_stmt() {
    // Nothing to do for void as a statement
    return 0;
}

int Void::gencode_expr(VarType expected_type, llvm::Value *&value) {
    // For void expressions, return null as value
    value = nullptr;
    return 0;
} 