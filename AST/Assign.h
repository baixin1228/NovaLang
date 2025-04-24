#pragma once
#include "ASTNode.h"
#include <iostream>

class Assign : public ASTNode {
public:
    std::string var;
    std::shared_ptr<ASTNode> value;
    bool need_create;
    bool is_global;

    Assign(Context &ctx, std::string v, std::shared_ptr<ASTNode> val, int ln)
        : ASTNode(ctx, ln), var(v), value(std::move(val)), need_create(false) {
            value->set_parent(this);
        }

    void print(int level) override {
      auto& var_info = lookup_var_info(var);
      std::cout << std::string(level * 2, ' ') << "Assign: " << var
                << " type:" << var_type_to_string(var_info.type) << " need_create:" << need_create << " is_global:" << is_global << " [行 " << line << "]\n";
      value->print(level + 1);
    }

    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
    int gencode_stmt() override;
    llvm::Value *gencode_expr(VarType expected_type) override;
};