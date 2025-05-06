#include <cstdint>
#include <dlfcn.h>
#include <iostream>
#include "CodeGen.h"
#include "Common.h"
#include "Context.h"
#include "Function.h"
#include "If.h"
#include "For.h"
#include "StructLiteral.h"
#include "While.h"

CodeGen::CodeGen(Context& ctx, bool debug) 
    : ctx(ctx),
      dbg_builder(nullptr),
      dbg_file(nullptr),
      dbg_compile_unit(nullptr),
      generate_debug_info(debug) {
    
    ctx.llvm_context = std::make_unique<llvm::LLVMContext>();
    ctx.module = std::make_unique<llvm::Module>("novalang", *ctx.llvm_context);
    ctx.builder = std::make_unique<llvm::IRBuilder<>>(*ctx.llvm_context);
    // 初始化运行时管理器
    ctx.runtime_manager =
    std::make_unique<RuntimeManager>(*ctx.llvm_context, ctx.module.get(), *ctx.builder);
  if (!ctx.runtime_manager->initialize()) {
    throw std::runtime_error("Failed to initialize runtime manager");
    }
    
    // 只有在需要生成调试信息时才初始化调试信息
    if (generate_debug_info) {
        dbg_builder = std::make_unique<llvm::DIBuilder>(*ctx.module);
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
        ctx.builder->getInt32Ty(),
        {ctx.builder->getInt8PtrTy()},
        true
    );
    ctx.printf_func = llvm::Function::Create(
        printf_ty,
        llvm::Function::ExternalLinkage,
        "printf",
        *ctx.module
    );
}

int CodeGen::generate_global_variable(Assign &assign) {
  auto var_info = assign.lookup_var(assign.var, assign.line);
  if (!var_info) {
    throw std::runtime_error("未定义的变量: " + assign.var +
                             " source:" + std::to_string(assign.line) +
                             " file:" + std::string(__FILE__) +
                             " line:" + std::to_string(__LINE__));
    return -1;
  }
  VarType type = var_info->node->type;

  if (assign.need_create) {
    llvm::Type *ty;
    switch (type) {
    case VarType::INT:
      ty = ctx.builder->getInt64Ty();
      break;
    case VarType::FLOAT:
      ty = ctx.builder->getDoubleTy();
      break;
    case VarType::BOOL:
      ty = ctx.builder->getInt1Ty();
      break;
    case VarType::STRING:
    case VarType::STRUCT:
    case VarType::DICT:
    case VarType::LIST:
      ty = llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0);
      break;
    default:
      ty = ctx.builder->getVoidTy();
      break;
    }

    std::cout << "==== Generating global variable: " << assign.var
              << "====" << std::endl;
    // 创建全局变量
    auto global_var =
        new llvm::GlobalVariable(*ctx.module, ty,
                                 false, // 是否为常量
                                 llvm::GlobalValue::ExternalLinkage,
                                 llvm::Constant::getNullValue(ty), // 初始值
                                 assign.var);
    global_var->setAlignment(llvm::Align(get_type_align(type)));

    var_info->llvm_obj = global_var;
  }

  if (auto *assign_value = dynamic_cast<Assign *>(assign.value.get())) {
    if (generate_global_variable(*assign_value) == -1) {
      return -1;
    }
  }
  return 0;
}

int CodeGen::generate() {
    auto &stmts = ctx.get_ast();
    // 然后生成所有函数的定义
    for (auto &stmt : stmts) {
      if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
        if (assign->gencode_var() == -1) {
          return -1;
        }
      }
      if (auto *iff = dynamic_cast<If *>(stmt.get())) {
        for (auto &stmt : iff->body) {
          if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
            if (assign->gencode_var() == -1) {
              return -1;
            }
          }
        }
      }
      if (auto *ffor = dynamic_cast<For *>(stmt.get())) {
        for (auto &stmt : ffor->body) {
          if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
            if (assign->gencode_var() == -1) {
              return -1;
            }
          }
        }
      }
      if (auto *whl = dynamic_cast<While *>(stmt.get())) {
        for (auto &stmt : whl->body) {
          if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
            if (assign->gencode_var() == -1) {
              return -1;
            }
          }
        }
      }
      if (auto *func = dynamic_cast<Function *>(stmt.get())) {
        std::cout << "==== Generating function: " << func->name << " ====" << std::endl;
        if (func->gencode_stmt() == -1) {
          return -1;
        }
      }

      if (auto *cls = dynamic_cast<StructLiteral *>(stmt.get())) {
        if (cls->struct_type == StructType::CLASS) {
          std::cout << "==== Generating class: " << cls->name << " ====" << std::endl;
          if (cls->gencode_stmt() == -1) {
            return -1;
          }
        }
      }
    }

    // 生成 main 函数
    auto func_ty = llvm::FunctionType::get(ctx.builder->getInt32Ty(), {}, false);
    auto main_func = llvm::Function::Create(func_ty, llvm::Function::ExternalLinkage, "main", *ctx.module);
    auto block = llvm::BasicBlock::Create(*ctx.llvm_context, "entry", main_func);
    // 从现在开始所有生成的指令都要放入这个基本块中
    ctx.builder->SetInsertPoint(block);
    ctx.current_function = main_func;

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
        if (dynamic_cast<Function*>(stmt.get())) {
          continue;
        }
        auto struct_node = dynamic_cast<StructLiteral *>(stmt.get());
        if (struct_node) {
          if(struct_node->struct_type == StructType::CLASS) {
            for (auto &attr : struct_node->attributes) {
              if (auto *assign = dynamic_cast<Assign *>(attr.get())) {
                if (assign->gencode_stmt() == -1) {
                  return -1;
                }
              }
            }
            continue;
          }
        }
        if (stmt->gencode_stmt() == -1) {
          return -1;
        }
    }

    ctx.builder->CreateRet(ctx.builder->getInt32(0));
    return 0;
}

void CodeGen::print_ir() {
    std::cout << "[IR]\n";
    ctx.module->print(llvm::outs(), nullptr);
}

CodeGen::~CodeGen() {
    if (generate_debug_info && dbg_builder) {
        dbg_builder->finalize();
    }
} 

void CodeGen::save_to_file(std::string& filename) {
    std::error_code EC;
    llvm::raw_fd_ostream OS(filename, EC);
    if (EC) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法打开文件: " + filename, 0, __FILE__, __LINE__);
        return;
    }
    ctx.module->print(OS, nullptr);
}

void CodeGen::compile_to_executable(std::string& output_filename, bool generate_object, bool generate_assembly) {
    // 初始化目标
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    // 验证模块
    std::string error;
    llvm::raw_string_ostream error_stream(error);
    if (llvm::verifyModule(*ctx.module, &error_stream)) {
        ctx.add_error_front(ErrorHandler::ErrorLevel::Other, "模块验证失败: " + error, -1, __FILE__, __LINE__);
        return;
    }

    // 设置目标机器
    auto target_triple = llvm::sys::getDefaultTargetTriple();
    ctx.module->setTargetTriple(target_triple);

    std::string target_error;
    auto target = llvm::TargetRegistry::lookupTarget(target_triple, target_error);
    if (!target) {
        ctx.add_error_front(ErrorHandler::ErrorLevel::Other, "无法查找目标: " + target_error, -1, __FILE__, __LINE__);
        return;
    }

    auto CPU = "generic";
    auto Features = "";
    llvm::TargetOptions opt;
    auto RM = llvm::Optional<llvm::Reloc::Model>();
    auto target_machine = target->createTargetMachine(target_triple, CPU, Features, opt, RM);

    ctx.module->setDataLayout(target_machine->createDataLayout());

    // 如果不是生成汇编或目标文件，我们需要一个临时目标文件
    std::string obj_file = generate_object ? output_filename : 
                          (output_filename + ".o");

    // 创建输出文件
    std::error_code EC;
    llvm::raw_fd_ostream dest(generate_assembly ? output_filename : obj_file, EC);
    if (EC) {
        ctx.add_error_front(ErrorHandler::ErrorLevel::Other, 
              "无法打开输出文件: " + (generate_assembly ? output_filename : obj_file), -1, __FILE__, __LINE__);
        return;
    }

    // 创建PassManager
    llvm::legacy::PassManager pass;
    pass.add(llvm::createVerifierPass());  // 添加验证Pass

    // 根据参数决定生成类型
    llvm::CodeGenFileType file_type = generate_assembly ? llvm::CGFT_AssemblyFile : llvm::CGFT_ObjectFile;
    
    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
        ctx.add_error_front(ErrorHandler::ErrorLevel::Other,
                        std::string("目标机器无法生成") +
                            (generate_assembly ? "汇编文件" : "目标文件"),
                        -1, __FILE__, __LINE__);
        return;
    }

    // 运行Pass
    pass.run(*ctx.module);
    dest.flush();

    // 如果不是生成汇编或目标文件，需要调用链接器生成可执行文件
    if (!generate_assembly && !generate_object) {
        // 构建链接命令
        std::string link_cmd = "ld -o " + output_filename + " " + obj_file;
        
        // 执行链接命令
        int result = std::system(link_cmd.c_str());
        if (result != 0) {
            ctx.add_error_front(ErrorHandler::ErrorLevel::Other, "链接失败", -1, __FILE__, __LINE__);
            return;
        }

        // 删除临时目标文件
        std::remove(obj_file.c_str());

        // 在Linux平台添加可执行权限
        if (PLATFORM_LINUX) {
            chmod(output_filename.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        }
    }
}

void CodeGen::execute() {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    // 验证模块
    std::string error;
    llvm::raw_string_ostream error_stream(error);
    if (llvm::verifyModule(*ctx.module, &error_stream)) {
      ctx.add_error_front(ErrorHandler::ErrorLevel::Other,
                          "模块验证失败: " + error, -1, __FILE__, __LINE__);
      return;
    }
    
    auto jit = llvm::orc::LLJITBuilder().create();
    if (!jit) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法创建 JIT", 0, __FILE__, __LINE__);
        return;
    }

    auto& main_jd = (*jit)->getMainJITDylib();
    auto dlsg = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
        (*jit)->getDataLayout().getGlobalPrefix());
    if (!dlsg) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法创建动态库搜索生成器", 0, __FILE__, __LINE__);
        return;
    }
    main_jd.addGenerator(std::move(*dlsg));

    // 创建符号映射
    llvm::orc::SymbolMap symbols;

    // 添加 printf 符号
    void* printf_ptr = dlsym(RTLD_DEFAULT, "printf");
    if (printf_ptr) {
        symbols[(*jit)->mangleAndIntern("printf")] = llvm::JITEvaluatedSymbol(
            llvm::pointerToJITTargetAddress(printf_ptr), llvm::JITSymbolFlags::Exported);
    }

    // 添加运行时库中的所有函数
    for (const auto& name : ctx.runtime_manager->getRuntimeFunctionNames()) {
        void* func_ptr = dlsym(ctx.runtime_manager->getRuntimeLibHandle(), name.c_str());
        if (func_ptr) {
            symbols[(*jit)->mangleAndIntern(name)] = llvm::JITEvaluatedSymbol(
                llvm::pointerToJITTargetAddress(func_ptr), llvm::JITSymbolFlags::Exported);
        } else {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                std::string("无法找到运行时函数 ") + name, 0, __FILE__, __LINE__);
            return;
        }
    }

    // 定义所有符号
    if (!symbols.empty()) {
        if (auto err = main_jd.define(llvm::orc::absoluteSymbols(symbols))) {
            std::string err_msg;
            llvm::raw_string_ostream err_stream(err_msg);
            err_stream << err;
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE, 
                "无法定义运行时符号: " + err_msg, 0, __FILE__, __LINE__);
            return;
        }
    }

    if (auto err = (*jit)->addIRModule(
            llvm::orc::ThreadSafeModule(std::move(ctx.module), std::make_unique<llvm::LLVMContext>()))) {
        std::string err_msg;
        llvm::raw_string_ostream err_stream(err_msg);
        err_stream << err;
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法添加模块: " + err_msg, 0, __FILE__, __LINE__);
        return;
    }

    auto main = (*jit)->lookup("main");
    if (!main) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法找到 main 函数", 0, __FILE__, __LINE__);
        return;
    }

    using MainFn = int(*)();
    auto main_fn = reinterpret_cast<MainFn>(main->getAddress());
    int result = main_fn();
}