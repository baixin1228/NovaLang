#pragma once
#include "ASTNode.h"
#include <iostream>
class BinOp : public ASTNode {
public:
    std::string op;
    std::shared_ptr<ASTNode> left, right;

    BinOp(Context &ctx, std::string o, std::shared_ptr<ASTNode> l,
          std::shared_ptr<ASTNode> r, int ln)
        : ASTNode(ctx, ln), op(o), left(std::move(l)), right(std::move(r)) {
        left->set_parent(this);
        right->set_parent(this);
    }

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "BinOp: " << op << " type:" << var_type_to_string(type) << " [行 " << line << "]\n";
        left->print(level + 1);
        right->print(level + 1);
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &self) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&ret_value) override;
    
private:
    // 类型检查函数
    int handle_equality_op();
    int handle_comparison_op();
    int handle_logical_op();
    int handle_arithmetic_op();
    int handle_power_op();
    
    // 辅助函数
    bool is_string_concat(VarType left_type, VarType right_type);
    
    // 代码生成函数 - 每个函数现在接收expected_type参数以支持隐式类型转换
    int gen_add(VarType expected_type, llvm::Value*& ret_value);
    int gen_subtract(VarType expected_type, llvm::Value*& ret_value);
    int gen_multiply(VarType expected_type, llvm::Value*& ret_value);
    int gen_divide(VarType expected_type, llvm::Value*& ret_value);
    int gen_floor_divide(VarType expected_type, llvm::Value*& ret_value);
    int gen_modulo(VarType expected_type, llvm::Value*& ret_value);
    int gen_power(VarType expected_type, llvm::Value*& ret_value);
    int gen_equal(VarType expected_type, llvm::Value*& ret_value);
    int gen_not_equal(VarType expected_type, llvm::Value*& ret_value);
    int gen_less_than(VarType expected_type, llvm::Value*& ret_value);
    int gen_greater_than(VarType expected_type, llvm::Value*& ret_value);
    int gen_greater_equal(VarType expected_type, llvm::Value*& ret_value);
    int gen_less_equal(VarType expected_type, llvm::Value*& ret_value);
    int gen_logical_and(VarType expected_type, llvm::Value*& ret_value);
    int gen_logical_or(VarType expected_type, llvm::Value*& ret_value);
};