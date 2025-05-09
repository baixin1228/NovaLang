#pragma once
#include "ASTNode.h"
#include <iostream>

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

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &expr_ret) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&value) override;
}; 