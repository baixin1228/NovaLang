#pragma once
#include "ASTNode.h"
#include "IndexAccess.h"
#include <iostream>

class IndexAssign : public ASTNode {
public:
    std::shared_ptr<ASTNode> container; // 容器表达式（数组或字典）
    std::shared_ptr<ASTNode> index;     // 索引表达式
    std::shared_ptr<ASTNode> value;     // 要赋予的值
    
    IndexAssign(Context &ctx, std::shared_ptr<ASTNode> c, 
               std::shared_ptr<ASTNode> i, std::shared_ptr<ASTNode> v, int ln)
        : ASTNode(ctx, ln), container(std::move(c)), 
          index(std::move(i)), value(std::move(v)) {
        // 设置父节点关系
        container->set_parent(this);
        index->set_parent(this);
        value->set_parent(this);
    }
    
    void print(int level) override {
        std::cout << std::string(level * 2, ' ') 
                  << "IndexAssign [line " << line << "]\n";
        std::cout << std::string((level + 1) * 2, ' ') << "Container:\n";
        container->print(level + 2);
        std::cout << std::string((level + 1) * 2, ' ') << "Index:\n";
        index->print(level + 2);
        std::cout << std::string((level + 1) * 2, ' ') << "Value:\n";
        value->print(level + 2);
    }
    
    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &expr_ret) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&ret_value) override;
    
    // Helper methods for code generation
    int gencode_list_assign(llvm::Value *list_val, llvm::Value *index_val, llvm::Value *value_val, llvm::Value *&ret_value);
    int gencode_dict_assign(llvm::Value *dict_val, llvm::Value *key_val, llvm::Value *value_val, llvm::Value *&ret_value);
}; 