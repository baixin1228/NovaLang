#pragma once
#include "ASTNode.h"
#include <iostream>

class Return : public ASTNode {
public:
    std::shared_ptr<ASTNode> value;

    Return(Context &ctx, std::shared_ptr<ASTNode> v, int ln)
        : ASTNode(ctx, ln), value(std::move(v)) {
      value->set_parent(this);
    }

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "Return" << " type:" << var_type_to_string(type) << " [行 " << line << "]\n";
        value->print(level + 1);
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &self) override;
    int gencode_stmt() override;
    llvm::Value *gencode_expr(VarType expected_type) override;
};