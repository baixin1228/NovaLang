#include <cstdint>
#include <dlfcn.h>
#include <iostream>
#include "CodeGen.h"
#include "Common.h"
#include "Context.h"

void CodeGen::save_to_file(std::string& filename) {
    std::error_code EC;
    llvm::raw_fd_ostream OS(filename, EC);
    if (EC) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法打开文件: " + filename, 0, __FILE__, __LINE__);
        return;
    }
    module->print(OS, nullptr);
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
    if (llvm::verifyModule(*module, &error_stream)) {
        ctx.add_error_front(ErrorHandler::ErrorLevel::Other, "模块验证失败: " + error, -1, __FILE__, __LINE__);
        return;
    }

    // 设置目标机器
    auto target_triple = llvm::sys::getDefaultTargetTriple();
    module->setTargetTriple(target_triple);

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

    module->setDataLayout(target_machine->createDataLayout());

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
    pass.run(*module);
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
    if (llvm::verifyModule(*module, &error_stream)) {
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
    for (const auto& name : runtime_manager->getRuntimeFunctionNames()) {
        void* func_ptr = dlsym(runtime_manager->getRuntimeLibHandle(), name.c_str());
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
            llvm::orc::ThreadSafeModule(std::move(module), std::make_unique<llvm::LLVMContext>()))) {
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