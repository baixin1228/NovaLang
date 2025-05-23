#pragma once
#include "ASTNode.h"
#include <iostream>

class BoolLiteral : public ASTNode {
public:
    bool value;

    BoolLiteral(Context &ctx, bool v, int ln)
        : ASTNode(ctx, ln), value(v) {}

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "BoolLiteral: " << (value ? "True" : "False")
                  << " [行 " << line << "]\n";
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &expr_ret) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&ret_value) override;
};