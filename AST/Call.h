#pragma once
#include "ASTNode.h"
#include <iostream>

class Call : public ASTNode {
public:
    std::string name;
    std::vector<std::shared_ptr<ASTNode>> args;
    std::shared_ptr<ASTNode> forward_expr;
    
    Call(Context &ctx, std::string n, std::vector<std::shared_ptr<ASTNode>> a,
         std::shared_ptr<ASTNode> forward_expr, int ln)
        : ASTNode(ctx, ln), 
          name(n), 
          args(std::move(a)),
          forward_expr(forward_expr){}

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "Call: " << name << " type:" << var_type_to_string(type) << " [行 " << line << "]\n";
        
        if (!args.empty()) {
            std::cout << std::string((level + 1) * 2, ' ') << "Arguments:\n";
            for (const auto& arg : args) {
                arg->print(level + 2);
            }
        }
        if (forward_expr)
          forward_expr->print(level + 1);
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &self) override;
    int visit_func_expr(std::shared_ptr<ASTNode> &self, std::shared_ptr<ASTNode> ast_node);
    int visit_struct_expr(std::shared_ptr<ASTNode> &self, std::shared_ptr<ASTNode> &field_value);
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&ret_value) override;
    int gencode_func_expr(VarType expected_type, llvm::Value *&ret_value);
    int gencode_struct_expr(VarType expected_type, llvm::Value *&ret_value);
}; 