#pragma once
#include "ASTNode.h"
#include <string>
#include <vector>
#include <utility>
#include "CodeGen.h"
class DictLiteral : public ASTNode {
public:
    // 字典项：键和值
    std::vector<std::pair<std::shared_ptr<ASTNode>, std::shared_ptr<ASTNode>>> items;
    VarType key_type;  // 键的类型
    VarType value_type;  // 值的类型

    DictLiteral(Context &ctx, 
                std::vector<std::pair<std::shared_ptr<ASTNode>, std::shared_ptr<ASTNode>>> i, 
                int ln)
        : ASTNode(ctx, ln), items(std::move(i)), key_type(VarType::NONE), value_type(VarType::NONE) {
        for (auto& item : items) {
            item.first->set_parent(this);
            item.second->set_parent(this);
        }
    }

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "DictLiteral [行 " << line << "]\n";
        for (size_t i = 0; i < items.size(); ++i) {
            std::cout << std::string((level + 1) * 2, ' ') << "Key " << i << ":\n";
            items[i].first->print(level + 2);
            std::cout << std::string((level + 1) * 2, ' ') << "Value " << i << ":\n";
            items[i].second->print(level + 2);
        }
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &self) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&ret_value) override;
}; 