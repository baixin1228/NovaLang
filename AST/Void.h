#pragma once
#include "ASTNode.h"

// Void类型的AST节点，用于表示空返回类型
class Void : public ASTNode {
public:
    Void(Context &ctx, int line);
    ~Void() override = default;

    void print(int level = 0) override;
    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &expr_ret) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&value) override;
}; 