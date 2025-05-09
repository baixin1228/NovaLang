#pragma once
#include "ASTNode.h"
#include <iostream>
#include <string>

class IndexAccess : public ASTNode {
public:
    std::shared_ptr<ASTNode> container; // 容器表达式（数组或字典）
    std::shared_ptr<ASTNode> index;     // 索引表达式
    
    IndexAccess(Context &ctx, std::shared_ptr<ASTNode> c, 
                std::shared_ptr<ASTNode> i, int ln)
        : ASTNode(ctx, ln), container(std::move(c)), index(std::move(i)) {
        // 设置父节点关系
        container->set_parent(this);
        index->set_parent(this);
    }
    
    void print(int level) override {
        std::cout << std::string(level * 2, ' ') 
                  << "IndexAccess " << var_type_to_string(type) << " [line " << line << "]\n";
        std::cout << std::string((level + 1) * 2, ' ') << "Container:\n";
        container->print(level + 2);
        std::cout << std::string((level + 1) * 2, ' ') << "Index:\n";
        index->print(level + 2);
    }
    
    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &expr_ret) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&ret_value) override;
    int gencode_list_access(llvm::Value *list_val, llvm::Value *index_val, llvm::Value *&ret_value);
    int gencode_dict_access(llvm::Value *dict_val, llvm::Value *index_val, llvm::Value *&ret_value);
}; 