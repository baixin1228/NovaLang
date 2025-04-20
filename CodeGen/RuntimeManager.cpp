#include "RuntimeManager.h"
#include <dlfcn.h>
#include <iostream>

RuntimeManager::RuntimeManager(llvm::LLVMContext& context, llvm::Module* module, llvm::IRBuilder<>& builder)
    : context(context), module(module), builder(builder), runtime_lib(nullptr), nova_memory_block_type(nullptr) {
}

RuntimeManager::~RuntimeManager() {
    if (runtime_lib) {
        dlclose(runtime_lib);
        runtime_lib = nullptr;
    }
}

void RuntimeManager::initRuntimeTypes() {
    // 创建 nova_memory_block 结构体类型
    nova_memory_block_type = llvm::StructType::create(context, "nova_memory_block");
    
    // 设置结构体成员：ref_count和数据区域
    std::vector<llvm::Type*> memory_block_elements = {
        builder.getInt32Ty(),                          // int32_t ref_count
        llvm::ArrayType::get(builder.getInt8Ty(), 0)   // char data[] - 柔性数组成员
    };
    
    nova_memory_block_type->setBody(memory_block_elements);
}

std::vector<std::string> RuntimeManager::getRuntimeFunctionNames() const {
    return {
        "create_string_from_system",
        "create_string_from_encoding",
        "create_string_from_chars",
        "string_to_system",
        "string_to_encoding",
        "print_string",
        "println_string",
        "concat_strings",
        "get_string_length",
        "nova_memory_retain",
        "nova_memory_release",
        "nova_memory_alloc",
        "nova_memory_get_data"
    };
}

llvm::FunctionType* RuntimeManager::createFunctionType(const std::string& name) {
    // Nova内存块指针类型
    auto memory_block_ptr_type = llvm::PointerType::get(nova_memory_block_type, 0);
    
    if (name == "nova_memory_alloc") {
        std::vector<llvm::Type*> params = {
            builder.getInt64Ty()  // size_t size
        };
        return llvm::FunctionType::get(
            memory_block_ptr_type,  // return type: nova_memory_block*
            params,
            false
        );
    }
    else if (name == "nova_memory_get_data") {
        std::vector<llvm::Type*> params = {
            memory_block_ptr_type  // nova_memory_block* block
        };
        return llvm::FunctionType::get(
            builder.getInt8PtrTy(),  // return type: void*
            params,
            false
        );
    }
    else if (name == "create_string_from_system" || name == "create_string_from_encoding") {
        std::vector<llvm::Type*> params = {
            builder.getInt8PtrTy()  // const char* str
        };
        if (name == "create_string_from_encoding") {
            params.push_back(builder.getInt8PtrTy());  // const char* encoding
        }
        return llvm::FunctionType::get(
            memory_block_ptr_type,  // return type: nova_memory_block*
            params,
            false
        );
    }
    else if (name == "create_string_from_chars") {
        std::vector<llvm::Type*> params = {
            llvm::PointerType::get(builder.getInt16Ty(), 0),  // const UChar* chars
            builder.getInt32Ty()                              // int32_t length
        };
        return llvm::FunctionType::get(
            memory_block_ptr_type,  // return type: nova_memory_block*
            params,
            false
        );
    }
    else if (name == "string_to_system" || name == "string_to_encoding") {
        std::vector<llvm::Type*> params = {
            memory_block_ptr_type  // const nova_memory_block* str
        };
        if (name == "string_to_encoding") {
            params.push_back(builder.getInt8PtrTy());  // const char* encoding
        }
        return llvm::FunctionType::get(
            builder.getInt8PtrTy(),  // return type: char*
            params,
            false
        );
    }
    else if (name == "print_string" || name == "println_string") {
        return llvm::FunctionType::get(
            builder.getVoidTy(),  // return type: void
            {memory_block_ptr_type},  // nova_memory_block* str
            false
        );
    }
    else if (name == "nova_memory_release") {
        return llvm::FunctionType::get(
            builder.getInt32Ty(),  // return type: int32_t
            {memory_block_ptr_type},  // nova_memory_block* str
            false
        );
    }
    else if (name == "nova_memory_retain") {
        return llvm::FunctionType::get(
            builder.getInt32Ty(),  // return type: int32_t
            {memory_block_ptr_type},  // nova_memory_block* str
            false
        );
    }
    else if (name == "get_string_length") {
        return llvm::FunctionType::get(
            builder.getInt32Ty(),  // return type: int32_t
            {memory_block_ptr_type},  // const nova_memory_block* str
            false
        );
    }
    else if (name == "concat_strings") {
        std::vector<llvm::Type*> params = {
            memory_block_ptr_type,  // const nova_memory_block* str1
            memory_block_ptr_type   // const nova_memory_block* str2
        };
        return llvm::FunctionType::get(
            memory_block_ptr_type,  // return type: nova_memory_block*
            params,
            false
        );
    }
    
    return nullptr;
}

bool RuntimeManager::initRuntimeFunctions() {
    auto function_names = getRuntimeFunctionNames();
    
    for (const auto& name : function_names) {
        // 检查函数是否已经存在
        if (runtime_functions.find(name) != runtime_functions.end()) {
            continue;
        }

        // 获取函数类型
        auto func_type = createFunctionType(name);
        if (!func_type) {
            std::cerr << "无法创建函数类型: " << name << std::endl;
            return false;
        }
        
        // 创建函数声明
        auto func = llvm::Function::Create(
            func_type,
            llvm::Function::ExternalLinkage,
            name,
            module
        );
        
        runtime_functions[name] = func;
    }
    
    return true;
}

bool RuntimeManager::initialize() {
    // 初始化类型
    initRuntimeTypes();
    
    // 加载运行时库
    runtime_lib = dlopen("libnovalang_runtime.so", RTLD_NOW | RTLD_GLOBAL);
    if (!runtime_lib) {
        std::cerr << "无法加载运行时库: " << dlerror() << std::endl;
        return false;
    }
    
    // 初始化函数
    if (!initRuntimeFunctions()) {
        dlclose(runtime_lib);
        runtime_lib = nullptr;
        return false;
    }
    
    return true;
}

llvm::Function* RuntimeManager::getRuntimeFunction(const std::string& name) {
    auto it = runtime_functions.find(name);
    if (it != runtime_functions.end()) {
        return it->second;
    }
    
    // 如果函数不存在，尝试创建它
    auto func_type = createFunctionType(name);
    if (!func_type) {
        return nullptr;
    }
    
    auto func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        name,
        module
    );
    
    runtime_functions[name] = func;
    return func;
} 