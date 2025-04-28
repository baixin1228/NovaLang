#pragma once
#include "ASTNode.h"
#include <vector>
#include <iostream>


class ListLiteral : public ASTNode {
public:
    // 列表元素
    std::vector<std::shared_ptr<ASTNode>> elements;
    VarType element_type;  // 元素类型

    ListLiteral(Context &ctx, std::vector<std::shared_ptr<ASTNode>> e, int ln)
        : ASTNode(ctx, ln), elements(std::move(e)), element_type(VarType::NONE) {
        for (auto& element : elements) {
            element->set_parent(this);
        }
    }

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "ListLiteral [行 " << line << "]\n";
        for (size_t i = 0; i < elements.size(); ++i) {
            std::cout << std::string((level + 1) * 2, ' ') << "Element " << i << ":\n";
            elements[i]->print(level + 2);
        }
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &self) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&ret_value) override;
}; 