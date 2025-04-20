#pragma once
#include "ASTNode.h"
#include "Common.h"
#include "Error.h"
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <llvm/IR/Value.h>

class Context {
private:
  std::map<std::string, std::pair<int, VarType>> var_types;
  std::map<std::string, std::pair<std::vector<VarType>, VarType>>
      func_types;
  ErrorHandler errors;
  std::map<std::string, uint64_t> funcline;
  std::map<std::string, llvm::Value*> global_symbols;
  std::vector<std::shared_ptr<ASTNode>> ast;
  std::string source_filename;
  //全局结构体定义
  std::map<std::string, std::shared_ptr<ASTNode>> global_structs;
public:
  Context() = default;
  ~Context() = default;

  int add_var_type(const std::string &name, int line, VarType type);
  VarType get_var_type(const std::string &name, int line);

  // 函数类型操作
  void set_func_type(const std::string &name,
                     const std::vector<VarType> &param_types,
                     VarType return_type);
  void set_func_return_type(const std::string &name,
                            VarType return_type);
  std::pair<std::vector<VarType>, VarType> &
  get_func_type(const std::string &name);
  bool has_func(const std::string &name) const;

  // 符号表操作
  void set_funcline(const std::string &name, uint64_t value);
  std::optional<uint64_t> get_funcline(const std::string &name) const;

  // AST操作
  void add_ast_node(std::shared_ptr<ASTNode> node);
  std::vector<std::shared_ptr<ASTNode>> &get_ast();
  const std::shared_ptr<ASTNode> &get_func(const std::string &name) const;

  // 错误处理
  void add_error(ErrorHandler::ErrorLevel level, const std::string &msg,
                 int line, const char* file, int call_line);
  void add_error_front(ErrorHandler::ErrorLevel level, const std::string &msg,
                 int line, const char* file, int call_line);
  bool has_errors() const;
  void print_errors() const;

  void set_source_filename(const std::string& filename);
  const std::string& get_source_filename() const;

  // 生成唯一的局部变量名
  std::string generate_local_var_name(const std::string& original_name, const ASTNode* node) const;
  
  //全局结构体
  void add_global_struct(const std::string& name, std::shared_ptr<ASTNode> node);
  std::shared_ptr<ASTNode> lookup_global_struct(const std::string& name) const;
  // 全局符号表操作
  void add_llvm_symbol(const std::string& name, llvm::Value* value);
  llvm::Value* lookup_llvm_symbol(const std::string& name) const;
  // int erase_llvm_symbol(const std::string& name);
};