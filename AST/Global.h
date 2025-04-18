#pragma once
#include "ASTNode.h"

class Global : public ASTNode {
public:
    std::vector<std::string> vars;

    Global(Context &ctx, std::vector<std::string> v, int ln)
        : ASTNode(ctx, ln), vars(std::move(v)) {}

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "Global: ";
        for (const auto& var : vars) {
            std::cout << var << " ";
        }
        std::cout << " [行 " << line << "]\n";
    }

    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
}; 