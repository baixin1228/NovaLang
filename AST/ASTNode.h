#pragma once
#include <string>
#include <map>
#include <set>
#include <optional>
#include "Common.h"
#include <llvm/IR/IRBuilder.h>

class Context;

namespace llvm {
    class Value;
}

class ASTNode;

// Added for custom type mapping
struct StructInfo {
  int line;
  std::string name;                // Class name
  std::string parent_class;        // Parent class name (if any)
  llvm::StructType *type;          // LLVM struct type for this class
  std::shared_ptr<ASTNode> ast;
  std::vector<std::string> attrs;  // List of attributes
  std::vector<VarType> attr_types; // Types of the attributes
};

class ASTNode : public std::enable_shared_from_this<ASTNode> {
protected:
  ASTNode *parent;
  std::map<std::string, std::shared_ptr<ASTNode>> vars;
  std::map<std::string, std::shared_ptr<ASTNode>> func_infos;
  std::map<std::string, StructInfo> struct_infos;
  std::map<std::string, llvm::Value *> symbols;
  std::set<std::string> global_vars;
  Context &ctx;
  bool is_scope;

  int _add_func(const std::string &name, std::shared_ptr<ASTNode> node);
  int _add_struct_info(const std::string &name);

public:
  int line;
  VarType type;
  llvm::Value *llvm_obj;
  ASTNode(Context &ctx, int ln, bool is_scope = false);
  virtual ~ASTNode() = default;

  virtual void print(int level = 0) = 0;
  virtual int visit_stmt() = 0;
  virtual int visit_expr(std::shared_ptr<ASTNode> &self) = 0;
  virtual int gencode_stmt() = 0;
  virtual llvm::Value *gencode_expr(VarType expected_type) = 0;
  void set_parent(ASTNode *p);

  // scope related methods
  int add_var(const std::string &name, std::shared_ptr<ASTNode> node, bool is_params = false);
  std::shared_ptr<ASTNode> lookup_var(const std::string &name, int p_line);
  int add_func(const std::string &name, std::shared_ptr<ASTNode> node);
  std::shared_ptr<ASTNode> lookup_func(const std::string &name);

  int add_struct_info(const std::string &name);
  StructInfo& lookup_struct_info(const std::string &name);
  ASTNode *lookup_ast_function(const std::string &name);
  ASTNode *lookup_ast_struct(const std::string &name);

  // global variable related methods
  void add_global_var(const std::string &name);
  bool is_global_var(const std::string &name) const;

  // get scope depth (for generating unique local variable name)
  int get_scope_depth() const;
  std::string get_scope_path() const;
};
