#pragma once
#include "ASTNode.h"
#include <iostream>

class UnaryOp : public ASTNode {
public:
    std::string op;
    std::shared_ptr<ASTNode> expr;

    UnaryOp(Context &ctx, std::string o, std::shared_ptr<ASTNode> e, int ln)
        : ASTNode(ctx, ln), op(o), expr(std::move(e)) {
        expr->set_parent(this);
    }

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "UnaryOp: " << op << " [行 " << line << "]\n";
        expr->print(level + 1);
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &self) override;
    int gencode_stmt() override;
    llvm::Value *gencode_expr(VarType expected_type) override;
}; 