#pragma once
#include "ASTNode.h"

class Variable : public ASTNode {
public:
    std::string name;

    Variable(Context &ctx, std::string n, int ln)
        : ASTNode(ctx, ln), name(n) {}

    void print(int level) override {
      std::cout << std::string(level * 2, ' ') << "Variable: " << name << " "
                << var_type_to_string(lookup_var_type(name)) << " [行 " << line
                << "]\n";
    }

    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
}; 