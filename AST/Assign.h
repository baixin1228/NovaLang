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
        : ASTNode(ctx, ln), var(v), value(std::move(val)), need_create(false),
          is_global(false) {
            if (!value) {
                throw std::runtime_error("Assign value is nullptr: " + var + " line: " + std::to_string(line));
            }
            value->set_parent(this);
        }

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "Assign:\"" << var
                << "\" type:" << var_type_to_string(type) << " create:" << need_create << " global:" << is_global << " [行 " << line << "]\n";
        value->print(level + 1);
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &expr_ret) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&ret_value) override;
    int gencode_var();
    static int gencode_assign(std::string name, std::shared_ptr<VarInfo> var_info, std::shared_ptr<ASTNode> value, llvm::Value *&ret_value);
};