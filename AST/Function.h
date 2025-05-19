#pragma once
#include "ASTNode.h"
#include <iostream>
#include <vector>
#include "Assign.h"
#include "Annotation.h"

class Function : public ASTNode {
public:
    std::string name;
    std::shared_ptr<ASTNode> return_ast = nullptr;
    uint32_t reference_count;

    llvm::FunctionType *llvm_type;
    std::vector<std::pair<std::string, std::shared_ptr<ASTNode>>> params;
    std::vector<std::shared_ptr<ASTNode>> body;
    
    std::vector<std::shared_ptr<Annotation>> annotations;
    
    bool is_abstract = false;
    bool is_init = false;

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
        type = VarType::FUNCTION;
    }

    void print(int level) override {
        // 判断是否是方法(通过parent)
        std::cout << std::string(level * 2, ' ') << "Function: " << name << " type:" << var_type_to_string(type);
        std::cout << " (vars:";
        for (const auto &var : vars) {
            std::cout << var.first << " ";
        }
        std::cout << ") [行 " << line << "]\n";
        
        // 打印注解
        if (!annotations.empty()) {
            std::cout << std::string((level + 1) * 2, ' ') << "Annotations:\n";
            for (const auto& anno : annotations) {
                std::cout << std::string((level + 2) * 2, ' ') << "@" << anno->name << "\n";
            }
        }
        
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

    // 检查是否有特定注解
    bool has_annotation(const std::string& name) const {
        for (const auto& anno : annotations) {
            if (anno->name == name) {
                return true;
            }
        }
        return false;
    }
    
    // 获取特定注解
    std::shared_ptr<Annotation> get_annotation(const std::string& name) const {
        for (const auto& anno : annotations) {
            if (anno->name == name) {
                return anno;
            }
        }
        return nullptr;
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &expr_ret) override;
    void GenLocalVar(Assign &assign);
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&value) override;
}; 