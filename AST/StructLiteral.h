#pragma once
#include "ASTNode.h"
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include "Function.h"
#include "Common.h"

// 定义结构体类型枚举
enum class StructType {
    STRUCT,  // 普通结构体
    CLASS    // 类
};

class StructLiteral : public ASTNode {
private:
    // 为结构体字段生成类型签名（实现在.cpp中）
    std::string generateTypeSignature(const std::shared_ptr<ASTNode>& node) const;
    
    // 为匿名结构体生成唯一名称（实现在.cpp中）
    std::string generateStructName() const;

public:
    std::string name;
    int reference_count;
    Function *init_method;
    std::map<std::string, std::shared_ptr<ASTNode>> fields;     // 结构体变量/实例变量
    std::vector<std::shared_ptr<ASTNode>> functions;            // 函数列表
    std::map<std::string, std::shared_ptr<ASTNode>> attributes; // 类属性
    StructType struct_type;                                     // 类型标识：结构体或类

    // 构造函数支持类名、函数列表和属性
    StructLiteral(Context &ctx, std::string name,
                 std::map<std::string, std::shared_ptr<ASTNode>> f, 
                 std::vector<std::shared_ptr<ASTNode>> funcs,
                 std::map<std::string, std::shared_ptr<ASTNode>> attrs,
                 StructType struct_type, int ln)
        : ASTNode(ctx, ln), name(std::move(name)), fields(std::move(f)),
          functions(std::move(funcs)), attributes(std::move(attrs)),
          struct_type(struct_type) {
        // 设置所有字段的父节点
        for (auto &field : fields) {
            if (field.second) {
                field.second->set_parent(this);
            }
        }
        // 设置所有函数的父节点
        for (auto &func : functions) {
            if (func) {
                func->set_parent(this);
            }
        }
        // 设置所有属性的父节点
        for (auto &attr : attributes) {
            if (attr.second) {
                attr.second->set_parent(this);
            }
        }
    }

    void print(int level) override {
        std::string type_str = (struct_type == StructType::CLASS) ? "Class" : "Struct";
        std::cout << std::string(level * 2, ' ') << type_str << "Literal: " << name << " [行 " << line << "]\n";
        
        if (!attributes.empty()) {
            std::cout << std::string((level + 1) * 2, ' ') << "Attributes:\n";
            for (const auto& attr : attributes) {
                std::cout << std::string((level + 2) * 2, ' ') << attr.first << ":\n";
                if (attr.second) {
                    attr.second->print(level + 3);
                } else {
                    std::cout << std::string((level + 3) * 2, ' ') << "null\n";
                }
            }
        }
        
        if (!functions.empty()) {
            std::cout << std::string((level + 1) * 2, ' ') << "Functions:\n";
            for (const auto& func : functions) {
                if (func) {
                    func->print(level + 2);
                }
            }
        }
        
        if (!fields.empty()) {
            std::cout << std::string((level + 1) * 2, ' ') << "Fields:\n";
            for (const auto& field : fields) {
                std::cout << std::string((level + 2) * 2, ' ') << field.first << ":\n";
                if (field.second) {
                    field.second->print(level + 3);
                } else {
                    std::cout << std::string((level + 3) * 2, ' ') << "null\n";
                }
            }
        }
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &self) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&value) override;
}; 