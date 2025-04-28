#pragma once
#include "ASTNode.h"
#include <iostream>
#include <vector>

class Function : public ASTNode {
public:
    std::string name;
    std::shared_ptr<ASTNode> return_ast;
    uint32_t reference_count;
    llvm::Function *llvm_obj;
    std::vector<std::pair<std::string, std::shared_ptr<ASTNode>>> params;
    std::vector<std::shared_ptr<ASTNode>> body;
    
    // 仅保留是否是构造方法
    bool is_init;

    // 统一的构造函数
    Function(Context &ctx, std::string n, std::vector<std::string> p,
             std::vector<std::shared_ptr<ASTNode>> b, int ln, bool is_constructor = false)
        : ASTNode(ctx, ln, true),  // 设置 is_scope 为 true
          name(n), 
          reference_count(0), 
          body(std::move(b)),
          is_init(is_constructor) {
        for (auto &param : p) {
            params.push_back(std::make_pair(param, nullptr));
        }
        // 设置所有 body 语句的父节点为当前函数
        for (auto &stmt : body) {
            if (stmt) {
                stmt->set_parent(this);
            }
        }

    }

    void print(int level) override {
        // 判断是否是方法(通过parent)
        std::cout << std::string(level * 2, ' ') << "Function: " << name << " type:" << var_type_to_string(type);
        std::cout << " (vars:";
        for (const auto &var : vars) {
            std::cout << var.first << " ";
        }
        std::cout << ") [行 " << line << "]\n";
        
        std::cout << std::string((level + 1) * 2, ' ') << "Params: ";
        for (const auto &param : params) {
            std::cout << param.first << " ";
        }
        std::cout << "\n";
        
        std::cout << std::string((level + 1) * 2, ' ') << "Body:\n";
        for (const auto& stmt : body) {
            stmt->print(level + 2);
        }
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &self) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&value) override;
}; 