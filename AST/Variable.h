#pragma once
#include "ASTNode.h"
#include <iostream>

class Variable : public ASTNode {
public:
  std::string name;

  Variable(Context &ctx, std::string n, int ln) : ASTNode(ctx, ln), name(n) {}

  void print(int level) override {
    std::cout << std::string(level * 2, ' ') << "Variable: " << name << " "
              << var_type_to_string(type) << " [行 " << line << "]\n";
    }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &self) override;
    int gencode_stmt() override;
    int gencode_expr(VarType expected_type, llvm::Value *&value) override;
    static int gencode_var_expr(Context &ctx, std::string name,
                                int line,
                                VarType expected_type,
                                llvm::Value *&value,
                                std::shared_ptr<VarInfo> var_info);
}; 