#include "CodeGen.h"
#include "ASTNode.h"
#include "Assign.h"
#include "Function.h"
#include "Global.h"
#include "If.h"
#include "For.h"
#include "While.h"

CodeGen::CodeGen(Context& ctx, bool debug) 
    : context(), 
      module(std::make_unique<llvm::Module>("novalang", context)),
      builder(context),
      ctx(ctx),
      current_function(nullptr),
      printf_func(nullptr),
      dbg_builder(nullptr),
      dbg_file(nullptr),
      dbg_compile_unit(nullptr),
      generate_debug_info(debug) {
    
    // 只有在需要生成调试信息时才初始化调试信息
    if (generate_debug_info) {
        dbg_builder = std::make_unique<llvm::DIBuilder>(*module);
        source_filename = ctx.get_source_filename();
        dbg_file = dbg_builder->createFile(source_filename, ".");
        dbg_compile_unit = dbg_builder->createCompileUnit(
            llvm::dwarf::DW_LANG_C, 
            dbg_file, 
            "NovaLang Compiler", 
            false, 
            "", 
            0
        );
    }

    // 初始化 printf 函数
    auto printf_ty = llvm::FunctionType::get(
        builder.getInt32Ty(),
        {builder.getInt8PtrTy()},
        true
    );
    printf_func = llvm::Function::Create(
        printf_ty,
        llvm::Function::ExternalLinkage,
        "printf",
        *module
    );
}

int CodeGen::generate() {
    auto &stmts = ctx.get_ast();
    // 然后生成所有函数的定义
    for (auto &stmt : stmts) {
      if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
        if (generate_global_variable(*assign) == -1) {
          return -1;
        }
      }
      if (auto *iff = dynamic_cast<If *>(stmt.get())) {
        for (auto &stmt : iff->body) {
          if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
            if (generate_global_variable(*assign) == -1) {
              return -1;
            }
          }
        }
      }
      if (auto *ffor = dynamic_cast<For *>(stmt.get())) {
        for (auto &stmt : ffor->body) {
          if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
            if (generate_global_variable(*assign) == -1) {
              return -1;
            }
          }
        }
      }
      if (auto *whl = dynamic_cast<While *>(stmt.get())) {
        for (auto &stmt : whl->body) {
          if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
            if (generate_global_variable(*assign) == -1) {
              return -1;
            }
          }
        }
      }
      if (auto *func = dynamic_cast<Function *>(stmt.get())) {
        if (generate_function(*func) == -1) {
          return -1;
        }
      }
    }

    // 生成 main 函数
    auto func_ty = llvm::FunctionType::get(builder.getInt32Ty(), {}, false);
    auto main_func = llvm::Function::Create(func_ty, llvm::Function::ExternalLinkage, "main", *module);
    auto block = llvm::BasicBlock::Create(context, "entry", main_func);
    // 从现在开始所有生成的指令都要放入这个基本块中
    builder.SetInsertPoint(block);
    current_function = main_func;

    // 只有在需要生成调试信息时才创建调试信息
    if (generate_debug_info) {
        auto dbg_func = dbg_builder->createFunction(
            dbg_file,
            "main",
            "main",
            dbg_file,
            0,
            dbg_builder->createSubroutineType(dbg_builder->getOrCreateTypeArray({})),
            0,
            llvm::DINode::FlagPrototyped,
            llvm::DISubprogram::SPFlagDefinition
        );
        main_func->setSubprogram(dbg_func);
    }

    for (auto& stmt : stmts) {
        if (!dynamic_cast<Function*>(stmt.get())) {
            if (visit_stmt(*stmt) == -1) {
              return -1;
            }
        }
    }

    builder.CreateRet(builder.getInt32(0));
    return 0;
}

void CodeGen::print_ir() {
    std::cout << "[IR]\n";
    module->print(llvm::outs(), nullptr);
}

CodeGen::~CodeGen() {
    if (generate_debug_info && dbg_builder) {
        dbg_builder->finalize();
    }
} 