#pragma once
#include "ASTNode.h"
#include <string>
#include <iostream>

class StructFieldAccess : public ASTNode {
public:
    std::shared_ptr<ASTNode> struct_expr;
    std::string field_name;
    Context &ctx;

    StructFieldAccess(Context &ctx, std::shared_ptr<ASTNode> expr, std::string field, int ln)
        : ASTNode(ctx, ln), struct_expr(std::move(expr)), field_name(std::move(field)), ctx(ctx) {
        struct_expr->set_parent(this);
    }

    void print(int level) override {
      VarType struct_type;
      visit_expr(struct_type);
      std::cout
          << std::string(level * 2, ' ') << "StructFieldAccess: " << field_name
          << " " << var_type_to_string(struct_type) << " [行 " << line << "]\n";
      struct_expr->print(level + 1);
    }

    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
    int gencode_stmt() override;
    llvm::Value *gencode_expr(VarType expected_type) override;
};