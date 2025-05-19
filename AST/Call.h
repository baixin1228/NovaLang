#pragma once
#include "ASTNode.h"
#include <iostream>
#include "StructLiteral.h"

class Call : public ASTNode {
public:
    std::string name;
    std::vector<std::shared_ptr<ASTNode>> args;
    std::shared_ptr<ASTNode> forward_expr = nullptr;
    std::shared_ptr<StructLiteral> instance = nullptr;
    std::shared_ptr<ASTNode> return_ast = nullptr;

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
        if (instance)
          instance->print(level + 1);
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &expr_ret) override;
    int visit_func_expr(std::shared_ptr<ASTNode> &expr_ret, std::shared_ptr<ASTNode> ast_node);
    int visit_class_expr(std::shared_ptr<ASTNode> &expr_ret,
                         std::shared_ptr<ASTNode> ast_node);
    int visit_func_or_class(std::shared_ptr<ASTNode> &expr_ret,
                              std::shared_ptr<ASTNode> ast_node);
    int visit_prev_expr(std::shared_ptr<ASTNode> &expr_ret, std::shared_ptr<ASTNode> &field_value);
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&ret_value) override;
    int gencode_call_expr(VarType expected_type, llvm::Value *&ret_value);
    int gencode_prev_expr(VarType expected_type, llvm::Value *&ret_value);
    int gencode_func_expr(VarType expected_type, llvm::Value *&ret_value);

    // Static method to find a function in a class hierarchy
    static int get_instance_func(Context &ctx, ASTNode &node, std::string class_name, 
                                std::string func_name, int line,
                                std::shared_ptr<ASTNode> &ret_ast);
}; 