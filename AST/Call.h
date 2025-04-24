#pragma once
#include "ASTNode.h"
#include <iostream>

class Call : public ASTNode {
public:
    std::string name;
    std::vector<std::shared_ptr<ASTNode>> args;

    Call(Context &ctx, std::string n, std::vector<std::shared_ptr<ASTNode>> a,
         int ln)
        : ASTNode(ctx, ln), name(n), args(std::move(a)) {}

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "Call: " << name << " [行 " << line << "]\n";
        for (const auto& arg : args) {
            arg->print(level + 1);
        }
    }

    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
    int gencode_stmt() override;
    llvm::Value *gencode_expr(VarType expected_type) override;
}; 