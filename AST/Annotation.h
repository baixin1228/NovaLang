#pragma once
#include "ASTNode.h"
#include <iostream>
#include <vector>

class Annotation : public ASTNode {
public:
    std::string name;
    std::vector<std::shared_ptr<ASTNode>> arguments;

    Annotation(Context &ctx, std::string n, std::vector<std::shared_ptr<ASTNode>> args, int ln)
        : ASTNode(ctx, ln), name(n), arguments(std::move(args)) {}

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "Annotation: @" << name << " [行 " << line << "]\n";
        for (auto &arg : arguments) {
            arg->print(level + 1);
        }
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &expr_ret) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&ret_value) override;
};