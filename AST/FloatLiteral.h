#pragma once
#include "ASTNode.h"
#include "CodeGen.h"

class FloatLiteral : public ASTNode {
public:
    double value;

    FloatLiteral(Context &ctx, double v, int ln) : ASTNode(ctx, ln), value(v) {}

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "FloatLiteral: " << value << " [行 " << line << "]\n";
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &self) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&ret_value) override;
};