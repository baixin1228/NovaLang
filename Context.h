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
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include "RuntimeManager.h"
#include "StructLiteral.h"

class Context {
private:
  ErrorHandler errors;
  std::string source_filename;

  std::map<std::string, std::shared_ptr<ASTNode>> global_vars;
  std::map<std::string, std::shared_ptr<ASTNode>> func_infos;
  std::map<std::string, StructInfo> struct_infos;

  std::vector<std::shared_ptr<ASTNode>> ast;
  std::map<std::string, std::shared_ptr<ASTNode>> global_structs;
  // global struct definition
public:
  Context() = default;
  ~Context() = default;

  llvm::Function *printf_func;
  llvm::Function *current_function;
  std::unique_ptr<llvm::IRBuilder<>> builder;
  std::unique_ptr<llvm::LLVMContext> llvm_context;
  std::unique_ptr<llvm::Module> module;
  // Runtime manager
  std::shared_ptr<RuntimeManager> runtime_manager;

  int add_global_var(const std::string &name, std::shared_ptr<ASTNode> node);
  std::shared_ptr<ASTNode> lookup_global_var(const std::string &name, int line);
  int add_global_func(const std::string &name, std::shared_ptr<ASTNode> node);
  std::shared_ptr<ASTNode> lookup_global_func(const std::string &name);
  
  int add_struct_info(const std::string &name);
  StructInfo& get_struct_info(const std::string &name);

  // AST operation
  void add_ast_node(std::shared_ptr<ASTNode> node);
  std::vector<std::shared_ptr<ASTNode>> &get_ast();

  // error handling
  void add_error(ErrorHandler::ErrorLevel level, const std::string &msg,
                 int line, const char* file, int call_line);
  void add_error_front(ErrorHandler::ErrorLevel level, const std::string &msg,
                 int line, const char* file, int call_line);
  bool has_errors() const;
  void print_errors() const;

  void set_source_filename(const std::string& filename);
  const std::string& get_source_filename() const;

  // generate unique local variable name
  std::string generate_local_var_name(const std::string& original_name, const ASTNode* node) const;
  
  // global struct
  void add_global_struct(const std::string& name, std::shared_ptr<ASTNode> node);
  std::shared_ptr<ASTNode> lookup_global_struct(const std::string& name) const;

  // Get LLVM type from VarType
  llvm::Type* get_llvm_type(VarType type) const;
};