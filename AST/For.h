#pragma once
#include "ASTNode.h"
#include <iostream>

class For : public ASTNode {
public:
    std::string iterator;
    std::shared_ptr<ASTNode> end;
    std::vector<std::shared_ptr<ASTNode>> body;

    For(Context &ctx, std::string it, std::shared_ptr<ASTNode> e,
        std::vector<std::shared_ptr<ASTNode>> b, int ln)
        : ASTNode(ctx, ln),
          iterator(std::move(it)), 
          end(std::move(e)),
          body(std::move(b)) {
        // 设置 end 表达式的父节点
        if (end) {
            end->set_parent(this);
        }
        // 设置所有 body 语句的父节点
        for (auto &stmt : body) {
            if (stmt) {
                stmt->set_parent(this);
            }
        }
    }

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "For: " << iterator << " [行 " << line << "]\n";
        std::cout << std::string((level + 1) * 2, ' ') << "End:\n";
        end->print(level + 2);
        std::cout << std::string((level + 1) * 2, ' ') << "Body:\n";
        for (const auto& stmt : body) {
            stmt->print(level + 2);
        }
    }

    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
    int gencode_stmt() override;
    llvm::Value *gencode_expr(VarType expected_type) override;
}; 