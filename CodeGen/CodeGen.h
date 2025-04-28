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
#include <memory>
#include <string>
#include <llvm/IR/GlobalVariable.h>

#ifdef __linux__
#define PLATFORM_LINUX 1
#include <sys/stat.h>
#else
#define PLATFORM_LINUX 0
#endif

#include "RuntimeManager.h"
#include "Context.h"
#include "Assign.h"
#include "Function.h"
// 代码生成
class CodeGen {
private:
  Context &ctx;
  
  // Debug info
  std::shared_ptr<llvm::DIBuilder> dbg_builder;
  llvm::DIFile *dbg_file;
  llvm::DICompileUnit *dbg_compile_unit;
  std::string source_filename; // 源文件名
  bool generate_debug_info;    // 新增：控制是否生成调试信息

  int generate_global_variable(Assign &assign);
public:
    explicit CodeGen(Context & ctx, bool debug = false);
    ~CodeGen();

    int generate();

    void print_ir();

    void execute();

    // 新增方法：文件调试支持
    void save_to_file(std::string & filename); // 保存IR到文件
    void compile_to_executable(
        std::string & output_filename, bool generate_object = false,
        bool generate_assembly = false); // 编译为可执行文件或目标文件
  };
