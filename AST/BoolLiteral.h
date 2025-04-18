#pragma once
#include "ASTNode.h"

class BoolLiteral : public ASTNode {
public:
    bool value;

    BoolLiteral(Context &ctx, bool v, int ln)
        : ASTNode(ctx, ln), value(v) {}

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "BoolLiteral: " << (value ? "True" : "False")
                  << " [行 " << line << "]\n";
    }

    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
}; 