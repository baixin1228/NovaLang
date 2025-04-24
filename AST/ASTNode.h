#pragma once
#include <string>
#include <map>
#include <set>
#include "Common.h"
#include <llvm/IR/IRBuilder.h>

namespace llvm {
    class Value;
}

class Context;

class ASTNode {
  protected:
    ASTNode* parent;
    std::map<std::string, VarInfo> var_infos;
    std::map<std::string, FuncInfo> func_infos;
    std::map<std::string, llvm::Value*> symbols;
    std::set<std::string> global_vars;
    Context &ctx;
    bool is_scope;

    int _add_func_info(const std::string &name);

  public:
    int line;
    ASTNode(Context &ctx, int ln, bool is_scope = false);
    virtual ~ASTNode() = default;
    
    virtual void print(int level = 0) = 0;
    virtual int visit_stmt(VarType &result) = 0;
    virtual int visit_expr(VarType &result) = 0;
    virtual int gencode_stmt() = 0;
    virtual llvm::Value *gencode_expr(VarType expected_type) = 0;
    void set_parent(ASTNode *p);

    // scope related methods
    virtual int add_var_info(const std::string& name, bool is_params = false);
    VarInfo& lookup_var_info(const std::string &name);
    int add_func_info(const std::string &name);
    FuncInfo &lookup_func_info(const std::string &name);
    void add_var_llvm_obj(const std::string &name, llvm::Value *value);
    llvm::Value* lookup_var_llvm_obj(const std::string &name);

    // global variable related methods
    virtual void add_global_var(const std::string& name);
    virtual bool is_global_var(const std::string& name) const;

    // get scope depth (for generating unique local variable name)
    int get_scope_depth() const;
    std::string get_scope_path() const;
};
