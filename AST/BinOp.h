#pragma once
#include "ASTNode.h"

class BinOp : public ASTNode {
public:
    std::string op;
    std::unique_ptr<ASTNode> left, right;

    BinOp(Context &ctx, std::string o, std::unique_ptr<ASTNode> l,
          std::unique_ptr<ASTNode> r, int ln)
        : ASTNode(ctx, ln), op(o), left(std::move(l)), right(std::move(r)) {
        left->set_parent(this);
        right->set_parent(this);
    }

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "BinOp: " << op << " [行 " << line << "]\n";
        left->print(level + 1);
        right->print(level + 1);
    }

    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
}; 