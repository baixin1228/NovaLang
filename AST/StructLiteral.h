#pragma once
#include "ASTNode.h"
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include "Function.h"
#include "Variable.h"
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
    int visit_count = 0;
    int reference_count = 0;
    Function *init_method = nullptr;
    std::vector<std::pair<std::string, std::shared_ptr<ASTNode>>> fields;     // 结构体变量/实例变量
    std::map<std::string, std::shared_ptr<ASTNode>> functions;  // 函数列表
    std::vector<std::shared_ptr<ASTNode>> attributes; // 类属性
    StructType struct_type;                 // 类型标识：结构体或类

    // 构造函数支持类名、函数列表和属性
    StructLiteral(Context &ctx, std::string name,
                 std::vector<std::pair<std::string, std::shared_ptr<ASTNode>>> f, 
                 std::vector<std::shared_ptr<ASTNode>> funcs,
                 std::vector<std::shared_ptr<ASTNode>> attrs,
                 StructType struct_type, int ln)
        : ASTNode(ctx, ln), name(std::move(name)), fields(std::move(f)),
          attributes(std::move(attrs)), struct_type(struct_type) {
        // 设置所有字段的父节点
        for (auto &field : fields) {
            if (field.second) {
                field.second->set_parent(this);
            }
        }
        // 设置所有函数的父节点
        for (auto &func_ast : funcs) {
            if (func_ast) {
                func_ast->set_parent(this);
            }
            auto func = std::dynamic_pointer_cast<Function>(func_ast);
            if (func) {
                functions[func->name] = func;
            } else {
                throw std::runtime_error("函数类型错误" + 
                var_type_to_string(func_ast->type)  + 
                " source_line:" + std::to_string(ln) + 
                " file:" + std::string(__FILE__) + 
                " line:" + std::to_string(__LINE__));
            }
        }
        // 设置所有属性的父节点
        for (auto &attr : attributes) {
            if (attr) {
                attr->set_parent(this);
                auto attr_node = std::dynamic_pointer_cast<Assign>(attr);
                if (attr_node) {
                    attr_node->is_global = true;
                }
            }
        }
        // 类类型需要设置为作用域
        if (struct_type == StructType::CLASS) {
          is_scope = true;
        }
    }

    void print(int level) override {
        std::string type_str = (struct_type == StructType::CLASS) ? "Class" : "Struct";
        std::cout << std::string(level * 2, ' ') << type_str << "Literal: " << name << " [行 " << line << "]\n";
        
        if (!attributes.empty()) {
            std::cout << std::string((level + 1) * 2, ' ') << "Attributes:\n";
            for (const auto& attr : attributes) {
                if (attr) {
                    attr->print(level + 2);
                } else {
                    std::cout << std::string((level + 2) * 2, ' ') << "null\n";
                }
            }
        }
        
        if (!functions.empty()) {
            std::cout << std::string((level + 1) * 2, ' ') << "Functions:\n";
            for (const auto& func : functions) {
                if (func.second) {
                    func.second->print(level + 2);
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