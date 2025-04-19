#pragma once
#include "ASTNode.h"
#include "Context.h"
#include <memory>
#include <string>

class CompoundAssign : public ASTNode {
public:
    std::string var;
    std::string op;
    std::unique_ptr<ASTNode> value;

    CompoundAssign(Context& ctx, std::string v, std::string o, std::unique_ptr<ASTNode> val, int ln)
        : ASTNode(ctx, ln), var(v), op(o), value(std::move(val)) {
            value->set_parent(this);
        }

    void print(int indent) override;
    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
}; 