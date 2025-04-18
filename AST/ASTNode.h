#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <optional>
#include <set>
#include "Common.h"

namespace llvm {
    class Value;
}

class Context;

class ASTNode {
  protected:
    ASTNode* parent;
    std::map<std::string, VarType> var_types;
    std::map<std::string, llvm::Value*> symbols;
    std::set<std::string> global_vars;
    Context &ctx;
    bool is_scope;

  public:
    int line;
    ASTNode(Context &ctx, int ln, bool is_scope = false);
    virtual ~ASTNode() = default;
    
    // 打印方法
    virtual void print(int level = 0) = 0;
    
    // 访问方法
    virtual int visit_stmt(VarType &result) = 0;
    virtual int visit_expr(VarType &result) = 0;
    void set_parent(ASTNode* p);

    // 变量作用域相关方法
    virtual int add_var_type(const std::string& name, VarType type, bool force = false);
    VarType lookup_var_type(const std::string &name);

    // 全局变量相关方法
    virtual void add_global_var(const std::string& name);
    virtual bool is_global_var(const std::string& name) const;

    // 获取作用域深度（用于生成唯一的局部变量名）
    int get_scope_depth() const;
    std::string get_scope_path() const;

    // 符号表操作
    void add_llvm_symbol(const std::string& name, llvm::Value* value);
    llvm::Value* lookup_llvm_symbol(const std::string& name) const;
    // int erase_llvm_symbol(const std::string& name);
};
