#pragma once
#include "ASTNode.h"
#include "Context.h"
#include <memory>
#include <string>

class CompoundAssign : public ASTNode {
public:
    std::string var;
    std::string op;
    std::shared_ptr<ASTNode> value;

    CompoundAssign(Context& ctx, std::string v, std::string o, std::shared_ptr<ASTNode> val, int ln)
        : ASTNode(ctx, ln), var(v), op(o), value(std::move(val)) {
            value->set_parent(this);
        }

    void print(int indent) override;
    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &self) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&ret_value) override;
}; 