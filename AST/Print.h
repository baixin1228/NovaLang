#pragma once
#include "ASTNode.h"
#include <memory>
#include <vector>
#include <iostream>

class Print : public ASTNode {
public:
    std::vector<std::shared_ptr<ASTNode>> values;

    Print(Context& ctx, std::shared_ptr<ASTNode> val, int ln) 
        : ASTNode(ctx, ln) {
            val->set_parent(this);
            values.push_back(std::move(val));
    }

    Print(Context& ctx, std::vector<std::shared_ptr<ASTNode>> vals, int ln)
        : ASTNode(ctx, ln), values(std::move(vals)) {
            for (auto& val : values) {
                val->set_parent(this);
            }
        }

    void print(int level = 0) override {
        for (int i = 0; i < level; i++) std::cout << "  ";
        std::cout << "Print:" << std::endl;
        for (const auto& val : values) {
            val->print(level + 1);
        }
    }

    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
    int gencode_stmt() override;
    llvm::Value *gencode_expr(VarType expected_type) override;
}; 