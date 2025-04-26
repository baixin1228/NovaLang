#pragma once
#include "ASTNode.h"
#include <iostream>

class StructFieldAssign : public ASTNode {
public:
    std::string field_name;
    std::shared_ptr<ASTNode> struct_expr;
    std::shared_ptr<ASTNode> value;

    StructFieldAssign(Context &ctx, std::string field, std::shared_ptr<ASTNode> expr, 
                 std::shared_ptr<ASTNode> val, int ln)
        : ASTNode(ctx, ln), field_name(field), struct_expr(std::move(expr)), value(std::move(val)) {
            struct_expr->set_parent(this);
            value->set_parent(this);
        }

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "StructFieldAssign: " << field_name
                << " " << var_type_to_string(type) << " [行 " << line << "]\n";
        std::cout << std::string((level + 1) * 2, ' ') << "Struct:\n";
        struct_expr->print(level + 2);
        std::cout << std::string((level + 1) * 2, ' ') << "Value:\n";
        value->print(level + 2);
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &self) override;
    int gencode_stmt() override;
    llvm::Value *gencode_expr(VarType expected_type) override;
}; 