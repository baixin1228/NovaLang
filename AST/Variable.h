#pragma once
#include "ASTNode.h"
#include <iostream>

class Variable : public ASTNode {
public:
    std::string name;

    Variable(Context &ctx, std::string n, int ln)
        : ASTNode(ctx, ln), name(n) {}

    void print(int level) override {
      auto& var_info = lookup_var_info(name);
      std::cout << std::string(level * 2, ' ') << "Variable: " << name << " "
                << var_type_to_string(var_info.type) << " [行 " << line
                << "]\n";
    }

    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
    int gencode_stmt() override;
    llvm::Value *gencode_expr(VarType expected_type) override;
}; 