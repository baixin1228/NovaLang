#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <string>
#include <vector>
#include <map>

class RuntimeManager {
private:
    llvm::LLVMContext& context;
    llvm::Module* module;
    llvm::IRBuilder<>& builder;
    void* runtime_lib;
    
    // 运行时类型
    llvm::StructType* nova_memory_block_type;
    
    // 运行时函数
    std::map<std::string, llvm::Function*> runtime_functions;
    
    // 初始化运行时类型
    void initRuntimeTypes();
    
    // 初始化运行时函数
    bool initRuntimeFunctions();

public:
    RuntimeManager(llvm::LLVMContext& context, llvm::Module* module, llvm::IRBuilder<>& builder);
    ~RuntimeManager();

    // 初始化运行时环境
    bool initialize();
    
    // 获取运行时类型
    llvm::StructType* getNovaMemoryBlockType() const { return nova_memory_block_type; }
    
    // 获取运行时函数
    llvm::Function* getRuntimeFunction(const std::string& name);
    
    // 获取运行时库句柄
    void* getRuntimeLibHandle() const { return runtime_lib; }
    
    // 创建运行时函数的LLVM函数类型
    llvm::FunctionType* createFunctionType(const std::string& name);
    
    // 获取所有运行时函数名称
    std::vector<std::string> getRuntimeFunctionNames() const;
}; 