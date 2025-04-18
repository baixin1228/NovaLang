#pragma once
#include "ASTNode.h"

class Increment : public ASTNode {
public:
    std::string var;

    Increment(Context &ctx, std::string v, int ln)
        : ASTNode(ctx, ln), var(v) {}

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "Increment: " << var << " [行 " << line << "]\n";
    }

    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
}; 