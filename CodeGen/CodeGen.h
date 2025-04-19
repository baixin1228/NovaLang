#pragma once
#include <llvm/CodeGen/Passes.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <llvm/IR/GlobalVariable.h>

#ifdef __linux__
#define PLATFORM_LINUX 1
#include <sys/stat.h>
#else
#define PLATFORM_LINUX 0
#endif

#include "ASTNode.h"
#include "Common.h"
#include "Context.h"
#include "Function.h"
#include "Assign.h"
#include "Print.h"
#include "RuntimeManager.h"
#include "StringLiteral.h"

// 获取类型对齐值
inline uint32_t get_type_align(VarType type) {
  switch (type) {
  case VarType::INT:
  case VarType::FLOAT:
    return 8;
  case VarType::BOOL:
    return 1;
  case VarType::STRING:
    return 8; // Pointer alignment
  default:
    std::cerr << "Unknown type: " << var_type_to_string(type) << std::endl;
    return 0;
  }
}

// 获取两个类型中较大的对齐值
inline uint32_t get_max_align(VarType type1, VarType type2) {
  return std::max(get_type_align(type1), get_type_align(type2));
}

// 代码生成
class CodeGen {
private:
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module;
  llvm::IRBuilder<> builder;
  std::map<std::string, llvm::Function *> functions;
  llvm::Function *printf_func; // Single printf declaration
  llvm::Function *current_function;
  Context &ctx;
  std::map<std::string, llvm::Value*> string_pool;  // 字符串池，用于复用相同的字符串
  
  // Debug info
  std::unique_ptr<llvm::DIBuilder> dbg_builder;
  llvm::DIFile *dbg_file;
  llvm::DICompileUnit *dbg_compile_unit;
  std::string source_filename; // 源文件名
  bool generate_debug_info;    // 新增：控制是否生成调试信息

  // Runtime manager
  std::unique_ptr<RuntimeManager> runtime_manager;

  void generatePrintStatement(Print *prt);
  
  // 处理字符串字面量，返回指向unicode字符串的指针
  llvm::Value* handleStringLiteral(StringLiteral* str_lit);

public:
  explicit CodeGen(Context &ctx, bool debug = false);
  ~CodeGen();

  int generate();

  void print_ir();

  int generate_function(Function &func);
  int generate_global_variable(Assign &assign);

  int visit_stmt(ASTNode &node);

  llvm::Value *visit_expr(ASTNode &node, VarType expected_type = VarType::NONE);

  VarType visit_expr_type(ASTNode &node);

  void execute();

  // 新增方法：文件调试支持
  void save_to_file(std::string &filename); // 保存IR到文件
  void compile_to_executable(
      std::string &output_filename, bool generate_object = false,
      bool generate_assembly = false); // 编译为可执行文件或目标文件

  void GenLocalVar(Assign& assign);

private:
  // ... existing code ...
};
