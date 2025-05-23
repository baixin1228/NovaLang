#pragma once
#include "ASTNode.h"
#include <iostream>

class IntLiteral : public ASTNode {
public:
    int value;

    IntLiteral(Context &ctx, int v, int ln)
        : ASTNode(ctx, ln), value(v) {}

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "IntLiteral: " << value << " [行 " << line << "]\n";
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &expr_ret) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&ret_value) override;
};