#pragma once
#include "ASTNode.h"
#include "StructLiteral.h"
#include <iostream>
#include <string>

int access_field_offset(std::shared_ptr<ASTNode> struct_node,
                        std::string field_name,
                        std::shared_ptr<ASTNode> &field_value,
                        size_t &field_offset, VarType &field_type);

class StructFieldAccess : public ASTNode {
public:
  std::shared_ptr<ASTNode> struct_expr = nullptr;
  StructType struct_type = StructType::STRUCT;
  std::string struct_signature;
  std::string field_name;
  Context &ctx;

  StructFieldAccess(Context &ctx, std::shared_ptr<ASTNode> expr,
                    std::string field, int ln)
      : ASTNode(ctx, ln), struct_expr(std::move(expr)),
        field_name(std::move(field)), ctx(ctx) {
    struct_expr->set_parent(this);
  }

  void print(int level) override {
    std::cout << std::string(level * 2, ' ')
              << "StructFieldAccess: " << field_name << " "
              << var_type_to_string(type) << " " << struct_signature << " [行 "
              << line << "]\n";
    if (struct_expr)
      struct_expr->print(level + 1);
  }

  int visit_stmt() override;
  int visit_struct_expr(std::shared_ptr<ASTNode> &self,
                        StructLiteral *struct_lit);
  int visit_class_expr(std::shared_ptr<ASTNode> &self,
                       StructLiteral *struct_lit);
  int visit_expr(std::shared_ptr<ASTNode> &self) override;
  int gencode_stmt() override;
  int gencode_expr(VarType expected_type, llvm::Value *&value) override;
  int gencode_struct_expr(VarType expected_type, llvm::Value *&value);
  int gencode_class_expr(VarType expected_type, llvm::Value *&value);
};