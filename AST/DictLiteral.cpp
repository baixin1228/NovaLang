#include "DictLiteral.h"
#include "TypeChecker.h"
#include "StringLiteral.h"

int DictLiteral::visit_stmt() {
    return 0;
}

int DictLiteral::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
    if (items.empty()) {
        return 0;
    }
    
    // 确定键的类型
    std::shared_ptr<ASTNode> key_ast;
    if (items[0].first->visit_expr(key_ast) == -1) {
        return -1;
    }
    
    // 确保所有键都是字符串类型
    if (key_ast->type != VarType::STRING) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                     "字典键必须是字符串类型，得到：" + var_type_to_string(key_ast->type),
                     line, __FILE__, __LINE__);
        return -1;
    }
    
    key_type = key_ast->type;
    
    // 确定值的类型
    std::shared_ptr<ASTNode> value_ast;
    if (items[0].second->visit_expr(value_ast) == -1) {
        return -1;
    }

    value_type = value_ast->type;

    // 确保所有键和值都有合适的类型
    for (size_t i = 1; i < items.size(); ++i) {
        std::shared_ptr<ASTNode> item_key_ast;
        if (items[i].first->visit_expr(item_key_ast) == -1) {
            return -1;
        }
        
        if (item_key_ast->type != key_type) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "字典键类型不一致，期望：" + var_type_to_string(key_type) + 
                         "，得到：" + var_type_to_string(item_key_ast->type),
                         line, __FILE__, __LINE__);
            return -1;
        }

        std::shared_ptr<ASTNode> item_value_ast;
        if (items[i].second->visit_expr(item_value_ast) == -1) {
          return -1;
        }

        if (item_value_ast->type != value_type) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "字典值类型不一致，期望：" + var_type_to_string(value_type) + 
                         "，得到：" + var_type_to_string(item_value_ast->type),
                         line, __FILE__, __LINE__);
            return -1;
        }
    }
    
    type = VarType::DICT;
    expr_ret = shared_from_this();
    return 0;
}

int DictLiteral::gencode_stmt() { return 0; }

int DictLiteral::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
    // 获取LLVM上下文和建造器
    llvm::LLVMContext &llvm_ctx = *ctx.llvm_context;
    llvm::IRBuilder<> &builder = *ctx.builder;
    llvm::Module *module = ctx.module.get();
    
    // 确定键值类型编号，对应于nova_dict.h中的类型编号
    // 键类型: 0=字符串, 1=整数, 2=浮点数
    uint8_t key_type_num = 0; // 字符串键
    
    // 值类型: 0=整数, 1=浮点数, 2=布尔, 3=指针
    uint8_t value_type_num;
    switch (value_type) {
        case VarType::INT:
            value_type_num = 0;
            break;
        case VarType::FLOAT:
            value_type_num = 1;
            break;
        case VarType::BOOL:
            value_type_num = 2;
            break;
        default: // 其他类型作为指针处理
            value_type_num = 3;
            break;
    }
    
    // 声明nova_dict_new函数
    llvm::FunctionType *dict_new_type = llvm::FunctionType::get(
        llvm::PointerType::get(llvm_ctx, 0), // 返回nova_memory_block指针
        {
            llvm::Type::getInt8Ty(llvm_ctx), // key_type
            llvm::Type::getInt8Ty(llvm_ctx)  // value_type
        },
        false);
    llvm::Function *dict_new_func = module->getFunction("nova_dict_new");
    if (!dict_new_func) {
        dict_new_func = llvm::Function::Create(
            dict_new_type,
            llvm::Function::ExternalLinkage,
            "nova_dict_new",
            module);
    }
    
    // 创建字典对象
    llvm::Value *dict_args[] = {
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(llvm_ctx), key_type_num),
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(llvm_ctx), value_type_num)
    };
    ret_value = builder.CreateCall(dict_new_func, dict_args, "dict");
    
    // 如果没有元素，直接返回空字典
    if (items.empty()) {
        return 0;
    }
    
    // 根据值类型声明适当的nova_dict_set_str_xxx函数
    llvm::FunctionType *dict_set_type;
    llvm::Function *dict_set_func;
    std::string func_name;
    
    // 根据值类型选择set函数
    switch (value_type) {
        case VarType::INT:
            func_name = "nova_dict_set_str_int";
            dict_set_type = llvm::FunctionType::get(
                llvm::Type::getVoidTy(llvm_ctx),
                {
                    llvm::PointerType::get(llvm_ctx, 0), // dict
                    llvm::PointerType::get(llvm_ctx, 0), // key
                    llvm::Type::getInt64Ty(llvm_ctx)     // value
                },
                false);
            break;
        case VarType::FLOAT:
            func_name = "nova_dict_set_str_float";
            dict_set_type = llvm::FunctionType::get(
                llvm::Type::getVoidTy(llvm_ctx),
                {
                    llvm::PointerType::get(llvm_ctx, 0), // dict
                    llvm::PointerType::get(llvm_ctx, 0), // key
                    llvm::Type::getDoubleTy(llvm_ctx)    // value
                },
                false);
            break;
        case VarType::BOOL:
            func_name = "nova_dict_set_str_bool";
            dict_set_type = llvm::FunctionType::get(
                llvm::Type::getVoidTy(llvm_ctx),
                {
                    llvm::PointerType::get(llvm_ctx, 0), // dict
                    llvm::PointerType::get(llvm_ctx, 0), // key
                    llvm::Type::getInt1Ty(llvm_ctx)      // value
                },
                false);
            break;
        default: // 指针类型
            func_name = "nova_dict_set_str_ptr";
            dict_set_type = llvm::FunctionType::get(
                llvm::Type::getVoidTy(llvm_ctx),
                {
                    llvm::PointerType::get(llvm_ctx, 0), // dict
                    llvm::PointerType::get(llvm_ctx, 0), // key
                    llvm::PointerType::get(llvm_ctx, 0)  // value
                },
                false);
            break;
    }
    
    dict_set_func = module->getFunction(func_name);
    if (!dict_set_func) {
        dict_set_func = llvm::Function::Create(
            dict_set_type,
            llvm::Function::ExternalLinkage,
            func_name,
            module);
    }
    
    // 添加所有键值对到字典
    for (size_t i = 0; i < items.size(); ++i) {
        // 生成键
        llvm::Value *key_value;
        if (items[i].first->gencode_expr(VarType::STRING, key_value) == -1) {
            return -1;
        }
        
        // 生成值
        llvm::Value *value_value;
        if (items[i].second->gencode_expr(value_type, value_value) == -1) {
            return -1;
        }
        
        // 调用添加函数
        llvm::Value *set_args[] = { ret_value, key_value, value_value };
        builder.CreateCall(dict_set_func, set_args);
    }
    
    return 0;
}