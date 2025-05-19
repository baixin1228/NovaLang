#pragma once
#include <string>
#include <map>
#include <set>
#include <optional>
#include "Common.h"
#include <llvm/IR/IRBuilder.h>

class Context;

class ASTNode;
struct VarInfo {
  int line;
  std::shared_ptr<ASTNode> node;
  llvm::Value *llvm_obj;
};

struct FuncInfo {
  std::shared_ptr<ASTNode> node;
  llvm::Function *llvm_func;
};

struct ClassInfo {
  std::shared_ptr<ASTNode> node;
  llvm::StructType *llvm_struct;
};

class ASTNode : public std::enable_shared_from_this<ASTNode> {
protected:
  std::map<std::string, std::shared_ptr<VarInfo>> vars;
  std::map<std::string, std::shared_ptr<FuncInfo>> funcs;
  std::map<std::string, std::shared_ptr<ClassInfo>> structs;
  std::map<std::string, llvm::Value *> symbols;
  std::set<std::string> global_vars;
  bool is_scope;

  int _add_func(const std::string &name, std::shared_ptr<FuncInfo> node);
  int _add_struct(const std::string &name, std::shared_ptr<ClassInfo> node);

public:
  int line;
  ASTNode *parent;
  VarType type = VarType::NONE;
  Context &ctx;
  ASTNode(Context &ctx, int ln, bool is_scope = false);
  virtual ~ASTNode() = default;

  virtual void print(int level = 0) = 0;
  virtual int visit_stmt() = 0;
  virtual int visit_expr(std::shared_ptr<ASTNode> &expr_ret) = 0;
  virtual int gencode_stmt() = 0;
  virtual int gencode_expr(VarType expected_type, llvm::Value *&value) = 0;
  void set_parent(ASTNode *p);

  // scope related methods
  int add_var(const std::string &name, std::shared_ptr<VarInfo> node, bool is_params = false);
  std::shared_ptr<VarInfo> lookup_var(const std::string &name, int p_line);
  int add_func(const std::string &name, std::shared_ptr<FuncInfo> node);
  std::shared_ptr<FuncInfo> lookup_func(const std::string &name);
  int add_struct(const std::string &name, std::shared_ptr<ClassInfo> node);
  std::shared_ptr<ClassInfo> lookup_struct(const std::string &name);

  // global variable related methods
  void add_global_var(const std::string &name);
  bool is_global_var(const std::string &name) const;

  // get scope depth (for generating unique local variable name)
  int get_scope_depth() const;
  std::string get_scope_path() const;
};
