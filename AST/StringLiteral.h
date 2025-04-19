#pragma once
#include "ASTNode.h"
#include "Context.h"
#include <string>
#include <iostream>

class StringLiteral : public ASTNode {
public:
    std::string value;

    StringLiteral(Context& ctx, const std::string& v, int ln) 
        : ASTNode(ctx, ln), value(v) {}

    void print(int level = 0) override {
        for (int i = 0; i < level; i++) {
            std::cout << "  ";
        }
        std::cout << "StringLiteral: \"" << value << "\"" << std::endl;
    }

    int visit_stmt(VarType &result) override;
    int visit_expr(VarType &result) override;
}; 