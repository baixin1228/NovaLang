#pragma once
#include "ASTNode.h"
#include <iostream>

class While : public ASTNode {
public:
    std::shared_ptr<ASTNode> condition;
    std::vector<std::shared_ptr<ASTNode>> body;

    While(Context &ctx, std::shared_ptr<ASTNode> cond,
          std::vector<std::shared_ptr<ASTNode>> b, int ln)
        : ASTNode(ctx, ln),
          condition(std::move(cond)), 
          body(std::move(b)) {
        // 设置条件表达式的父节点
        if (condition) {
            condition->set_parent(this);
        }
        // 设置所有 body 语句的父节点
        for (auto &stmt : body) {
            if (stmt) {
                stmt->set_parent(this);
            }
        }
    }

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "While: [行 " << line << "]\n";
        std::cout << std::string((level + 1) * 2, ' ') << "Condition:\n";
        condition->print(level + 2);
        std::cout << std::string((level + 1) * 2, ' ') << "Body:\n";
        for (const auto& stmt : body) {
            stmt->print(level + 2);
        }
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &expr_ret) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&value) override;
}; 