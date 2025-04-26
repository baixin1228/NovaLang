#pragma once
#include "ASTNode.h"
#include <iostream>

class If : public ASTNode {
public:
    std::shared_ptr<ASTNode> condition;
    std::vector<std::shared_ptr<ASTNode>> body;
    std::vector<std::pair<std::shared_ptr<ASTNode>, std::vector<std::shared_ptr<ASTNode>>>> elifs;
    std::vector<std::shared_ptr<ASTNode>> else_body;

    If(Context &ctx, std::shared_ptr<ASTNode> c,
       std::vector<std::shared_ptr<ASTNode>> b,
       std::vector<std::pair<std::shared_ptr<ASTNode>,
                             std::vector<std::shared_ptr<ASTNode>>>>
           e,
       std::vector<std::shared_ptr<ASTNode>> eb, int ln)
        : ASTNode(ctx, ln),
          condition(std::move(c)), 
          body(std::move(b)),
          elifs(std::move(e)), 
          else_body(std::move(eb)) {
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
        // 设置所有 elif 条件和语句的父节点
        for (auto &elif : elifs) {
            if (elif.first) {
                elif.first->set_parent(this);
            }
            for (auto &stmt : elif.second) {
                if (stmt) {
                    stmt->set_parent(this);
                }
            }
        }
        // 设置所有 else 语句的父节点
        for (auto &stmt : else_body) {
            if (stmt) {
                stmt->set_parent(this);
            }
        }
    }

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "If: [行 " << line << "]\n";
        std::cout << std::string((level + 1) * 2, ' ') << "Condition:\n";
        condition->print(level + 2);
        std::cout << std::string((level + 1) * 2, ' ') << "Body:\n";
        for (const auto& stmt : body) {
            stmt->print(level + 2);
        }
        if (!elifs.empty()) {
            std::cout << std::string((level + 1) * 2, ' ') << "Elif:\n";
            for (const auto& elif : elifs) {
                std::cout << std::string((level + 2) * 2, ' ') << "Condition:\n";
                elif.first->print(level + 3);
                std::cout << std::string((level + 2) * 2, ' ') << "Body:\n";
                for (const auto& stmt : elif.second) {
                    stmt->print(level + 3);
                }
            }
        }
        if (!else_body.empty()) {
            std::cout << std::string((level + 1) * 2, ' ') << "Else:\n";
            for (const auto& stmt : else_body) {
                stmt->print(level + 2);
            }
        }
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &self) override;
    int gencode_stmt() override;
    llvm::Value *gencode_expr(VarType expected_type) override;
}; 