#pragma once
#include "ASTNode.h"

class FloatLiteral : public ASTNode {
public:
    double value;

    FloatLiteral(Context &ctx, double v, int ln) : ASTNode(ctx, ln), value(v) {}

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "FloatLiteral: " << value << " [行 " << line << "]\n";
    }

    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
}; 