#pragma once
#include "ASTNode.h"
#include <iostream>

class Increment : public ASTNode {
public:
    std::string var;

    Increment(Context &ctx, std::string v, int ln)
        : ASTNode(ctx, ln), var(v) {}

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "Increment: " << var << " [行 " << line << "]\n";
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &self) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&ret_value) override;
}; 