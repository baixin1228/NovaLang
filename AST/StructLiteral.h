#pragma once
#include "ASTNode.h"
#include <string>
#include <vector>
#include <utility>
#include <iostream>

class StructLiteral : public ASTNode {
public:
    // 结构体字段：字段名和对应的值
    std::map<std::string, std::shared_ptr<ASTNode>> fields;

    StructLiteral(Context &ctx, std::map<std::string, std::shared_ptr<ASTNode>> f, int ln)
        : ASTNode(ctx, ln), fields(std::move(f)) {
        for (auto& field : fields) {
            field.second->set_parent(this);
        }
    }

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "StructLiteral [行 " << line << "]\n";
        for (const auto& field : fields) {
            std::cout << std::string((level + 1) * 2, ' ') << field.first << ":\n";
            field.second->print(level + 2);
        }
    }

    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
    int gencode_stmt() override;
    llvm::Value *gencode_expr(VarType expected_type) override;
}; 