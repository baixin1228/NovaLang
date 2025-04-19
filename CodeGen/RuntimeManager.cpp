#include "RuntimeManager.h"
#include <dlfcn.h>
#include <iostream>

RuntimeManager::RuntimeManager(llvm::LLVMContext& context, llvm::Module* module, llvm::IRBuilder<>& builder)
    : context(context), module(module), builder(builder), runtime_lib(nullptr), unicode_string_type(nullptr) {
}

RuntimeManager::~RuntimeManager() {
    if (runtime_lib) {
        dlclose(runtime_lib);
        runtime_lib = nullptr;
    }
}

void RuntimeManager::initRuntimeTypes() {
    // 创建 unicode_string 结构体类型 - 使用柔性数组成员
    unicode_string_type = llvm::StructType::create(context, "unicode_string");
    std::vector<llvm::Type*> unicode_string_elements = {
        builder.getInt32Ty(),                          // int32_t length
        llvm::ArrayType::get(builder.getInt16Ty(), 0)  // UChar data[] - 柔性数组成员
    };
    unicode_string_type->setBody(unicode_string_elements);
}

std::vector<std::string> RuntimeManager::getRuntimeFunctionNames() const {
    return {
        "create_unicode_string_from_system",
        "create_unicode_string_from_encoding",
        "unicode_string_to_system",
        "unicode_string_to_encoding",
        "free_unicode_string",
        "print_unicode_string",
        "println_unicode_string",
        "concat_unicode_strings"
    };
}

llvm::FunctionType* RuntimeManager::createFunctionType(const std::string& name) {
    if (name == "create_unicode_string_from_system" || name == "create_unicode_string_from_encoding") {
        std::vector<llvm::Type*> params = {
            builder.getInt8PtrTy()  // const char* str
        };
        if (name == "create_unicode_string_from_encoding") {
            params.push_back(builder.getInt8PtrTy());  // const char* encoding
        }
        return llvm::FunctionType::get(
            llvm::PointerType::get(unicode_string_type, 0),  // return type: unicode_string*
            params,
            false
        );
    }
    else if (name == "unicode_string_to_system" || name == "unicode_string_to_encoding") {
        std::vector<llvm::Type*> params = {
            llvm::PointerType::get(unicode_string_type, 0)  // const unicode_string* str
        };
        if (name == "unicode_string_to_encoding") {
            params.push_back(builder.getInt8PtrTy());  // const char* encoding
        }
        return llvm::FunctionType::get(
            builder.getInt8PtrTy(),  // return type: char*
            params,
            false
        );
    }
    else if (name == "free_unicode_string") {
        return llvm::FunctionType::get(
            builder.getVoidTy(),  // return type: void
            {llvm::PointerType::get(unicode_string_type, 0)},  // unicode_string* str
            false
        );
    }
    else if (name == "print_unicode_string" || name == "println_unicode_string") {
        return llvm::FunctionType::get(
            builder.getVoidTy(),  // return type: void
            {llvm::PointerType::get(unicode_string_type, 0)},  // const unicode_string* str
            false
        );
    }
    else if (name == "concat_unicode_strings") {
        std::vector<llvm::Type*> params = {
            llvm::PointerType::get(unicode_string_type, 0),  // const unicode_string* str1
            llvm::PointerType::get(unicode_string_type, 0)   // const unicode_string* str2
        };
        return llvm::FunctionType::get(
            llvm::PointerType::get(unicode_string_type, 0),  // return type: unicode_string*
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