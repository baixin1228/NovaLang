#pragma once
#include "ASTNode.h"
#include <iostream>

class Function : public ASTNode {
public:
    std::string name;
    uint32_t reference_count;
    std::vector<std::string> params;
    std::vector<std::shared_ptr<ASTNode>> body;

    Function(Context &ctx, std::string n, std::vector<std::string> p,
             std::vector<std::shared_ptr<ASTNode>> b, int ln)
        : ASTNode(ctx, ln, true),  // 设置 is_scope 为 true
          name(n), 
          reference_count(0), 
          params(std::move(p)),
          body(std::move(b)) {
        // 设置所有 body 语句的父节点为当前函数
        for (auto &stmt : body) {
            if (stmt) {
                stmt->set_parent(this);
            }
        }
    }

    void print(int level) override {
      std::cout << std::string(level * 2, ' ') << "Function: " << name
                << " (var:";
      for (const auto &var : var_infos) {
        std::cout << var.first << " ";
      }
      std::cout << ") [行 " << line << "]\n";
      std::cout << std::string((level + 1) * 2, ' ') << "Params: ";
      for (const auto &param : params) {
        std::cout << param << " ";
        }
        std::cout << "\n";
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