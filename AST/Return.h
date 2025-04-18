#pragma once
#include "ASTNode.h"

class Return : public ASTNode {
public:
    std::unique_ptr<ASTNode> value;

    Return(Context &ctx, std::unique_ptr<ASTNode> v, int ln)
        : ASTNode(ctx, ln), value(std::move(v)) {
      value->set_parent(this);
    }

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "Return [行 " << line << "]\n";
        value->print(level + 1);
    }

    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
}; 