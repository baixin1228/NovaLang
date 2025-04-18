#pragma once
#include "ASTNode.h"

class Assign : public ASTNode {
public:
    std::string var;
    std::unique_ptr<ASTNode> value;
    bool need_create;
    bool is_global;

    Assign(Context &ctx, std::string v, std::unique_ptr<ASTNode> val, int ln)
        : ASTNode(ctx, ln), var(v), value(std::move(val)), need_create(false) {
            value->set_parent(this);
        }

    void print(int level) override {
      std::cout << std::string(level * 2, ' ') << "Assign: " << var
                << " type:" << var_type_to_string(lookup_var_type(var)) << " need_create:" << need_create << " is_global:" << is_global << " [行 " << line << "]\n";
      value->print(level + 1);
    }

    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
};