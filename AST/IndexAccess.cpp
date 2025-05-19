#include "IndexAccess.h"
#include "TypeChecker.h"
#include "ListLiteral.h"
#include "DictLiteral.h"
#include "IntLiteral.h"

int IndexAccess::visit_stmt() {
    // 索引访问作为语句没有独立意义
    return 0;
}

int IndexAccess::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
    // 首先处理容器表达式
    std::shared_ptr<ASTNode> container_ast;
    if (container->visit_expr(container_ast) != 0) {
        return -1;
    }
    
    // 处理索引表达式
    std::shared_ptr<ASTNode> index_ast;
    if (index->visit_expr(index_ast) != 0) {
        return -1;
    }
    
    // 根据容器类型确定访问类型
    if (container_ast->type == VarType::LIST) {
        // 数组索引必须是整数
        if (index_ast->type != VarType::INT) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "Array index must be an integer, got: " + var_type_to_string(index_ast->type),
                         line, __FILE__, __LINE__);
            return -1;
        }
        
        auto int_literal = dynamic_cast<IntLiteral*>(index_ast.get());
        if (!int_literal) {
            throw std::runtime_error("IndexAccess: 内部错误，索引不是整数" + std::to_string(line) + " " + std::string(__FILE__) + " " + std::to_string(__LINE__));
        }
        auto list_literal = dynamic_cast<ListLiteral*>(container_ast.get());
        if (list_literal) {
            list_literal->elements[int_literal->value]->visit_expr(expr_ret);
            type = expr_ret->type;
            return 0;
        } else {
            throw std::runtime_error("IndexAccess: 内部错误，容器不是列表" + std::to_string(line) + " " + std::string(__FILE__) + " " + std::to_string(__LINE__));
        }
    }
    else if (container_ast->type == VarType::DICT) {
        // 字典键通常是字符串，但我们可以允许其他类型作为键
        // 设置访问表达式的类型为字典值类型
        auto dict_literal = dynamic_cast<DictLiteral*>(container_ast.get());
        if (dict_literal) {
            dict_literal->items[0].second->visit_expr(expr_ret);
            type = expr_ret->type;
            return 0;
        } else {
            throw std::runtime_error("IndexAccess: 内部错误，容器不是字典" + std::to_string(line) + " " + std::string(__FILE__) + " " + std::to_string(__LINE__));
        }
    }
    else {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                     "Cannot use index access on non-container type: " + var_type_to_string(container_ast->type),
                     line, __FILE__, __LINE__);
    }
    
    return -1;
}

int IndexAccess::gencode_stmt() {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                 "索引访问作为语句没有意义",
                 line, __FILE__, __LINE__);
    return -1;
}

int IndexAccess::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
    // 获取容器值
    llvm::Value *container_val = nullptr;
    if (container->gencode_expr(VarType::NONE, container_val) != 0) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                     "无法生成容器表达式的代码",
                     line, __FILE__, __LINE__);
        return -1;
    }

    // 获取索引值
    llvm::Value *index_val = nullptr;
    if (index->gencode_expr(VarType::INT, index_val) != 0) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                     "无法生成索引表达式的代码",
                     line, __FILE__, __LINE__);
        return -1;
    }

    // 确定访问的是列表还是字典
    std::shared_ptr<ASTNode> container_ast;
    if (container->visit_expr(container_ast) != 0) {
        return -1;
    }

    if (container_ast->type == VarType::LIST) {
        // 处理列表索引访问
        return gencode_list_access(container_val, index_val, ret_value);
    }
    else if (container_ast->type == VarType::DICT) {
        // 处理字典索引访问
        return gencode_dict_access(container_val, index_val, ret_value);
    }
    
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                 "不支持的容器类型进行索引访问: " + var_type_to_string(container_ast->type),
                 line, __FILE__, __LINE__);
    return -1;
}

int IndexAccess::gencode_list_access(llvm::Value *list_val, llvm::Value *index_val, llvm::Value *&ret_value) {
    // 获取列表类型信息
    std::shared_ptr<ASTNode> container_ast;
    container->visit_expr(container_ast);
    auto list_literal = std::dynamic_pointer_cast<ListLiteral>(container_ast);
    
    // 获取数据区域指针
    auto data_ptr = ctx.builder->CreateCall(
        ctx.runtime_manager->getRuntimeFunction("nova_memory_get_data"),
        {list_val});
    auto byte_ptr = ctx.builder->CreateBitCast(
        data_ptr, llvm::PointerType::get(ctx.builder->getInt8Ty(), 0));

    // 读取列表大小
    auto count_ptr = ctx.builder->CreateBitCast(
        byte_ptr, llvm::PointerType::get(ctx.builder->getInt64Ty(), 0));
    auto count = ctx.builder->CreateLoad(ctx.builder->getInt64Ty(), count_ptr, "list_size");
    
    // 检查索引是否越界
    auto out_of_bounds = ctx.builder->CreateICmpUGE(index_val, count, "out_of_bounds");
    
    // 创建基本块用于处理越界情况
    llvm::Function* curr_func = ctx.builder->GetInsertBlock()->getParent();
    auto normal_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "index_normal", curr_func);
    auto error_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "index_error", curr_func);
    
    ctx.builder->CreateCondBr(out_of_bounds, error_bb, normal_bb);
    
    // 处理越界情况
    ctx.update_insert_point(error_bb);
    auto err_msg = ctx.builder->CreateGlobalStringPtr("错误: 数组索引越界", "err_msg");
    auto printf_func = ctx.printf_func;
    auto err_fmt = ctx.builder->CreateGlobalStringPtr("%s\n", "err_fmt");
    std::vector<llvm::Value*> err_args = {err_fmt, err_msg};
    ctx.builder->CreateCall(printf_func, err_args);
    
    // 直接调用exit函数退出程序
    auto exit_func = ctx.module->getFunction("exit");
    if (!exit_func) {
        // 如果exit函数未定义，则创建它
        llvm::FunctionType *exit_type = llvm::FunctionType::get(
            ctx.builder->getVoidTy(),
            {ctx.builder->getInt32Ty()},
            false);
        exit_func = llvm::Function::Create(
            exit_type, 
            llvm::Function::ExternalLinkage, 
            "exit", 
            ctx.module.get());
    }
    
    // 调用exit(1)表示错误退出
    ctx.builder->CreateCall(exit_func, {ctx.builder->getInt32(1)});
    
    // 创建一个不可达的终止指令
    ctx.builder->CreateUnreachable();
    
    // 正常访问
    ctx.update_insert_point(normal_bb);
    
    VarType element_type = VarType::NONE;
    if (list_literal) {
        element_type = list_literal->element_type;
    }
    
    // 计算元素偏移量 - 开始位置是在元素数量之后
    auto elem_size = ctx.builder->getInt64(8); // 假设每个元素8字节
    auto offset_base = ctx.builder->getInt64(sizeof(size_t));
    auto index_offset = ctx.builder->CreateMul(index_val, elem_size);
    auto offset_val = ctx.builder->CreateAdd(offset_base, index_offset, "elem_offset");
    
    // 获取元素指针
    auto elem_ptr_ptr = ctx.builder->CreateGEP(
        ctx.builder->getInt8Ty(), byte_ptr, offset_val, "elem_ptr_ptr");
    
    // 根据元素类型加载值
    switch (element_type) {
    case VarType::INT: {
        auto elem_ptr = ctx.builder->CreateBitCast(
            elem_ptr_ptr, llvm::PointerType::get(ctx.builder->getInt64Ty(), 0));
        ret_value = ctx.builder->CreateLoad(ctx.builder->getInt64Ty(), elem_ptr);
        break;
    }
    case VarType::FLOAT: {
        auto elem_ptr = ctx.builder->CreateBitCast(
            elem_ptr_ptr, llvm::PointerType::get(ctx.builder->getDoubleTy(), 0));
        ret_value = ctx.builder->CreateLoad(ctx.builder->getDoubleTy(), elem_ptr);
        break;
    }
    case VarType::BOOL: {
        auto elem_ptr = ctx.builder->CreateBitCast(
            elem_ptr_ptr, llvm::PointerType::get(ctx.builder->getInt1Ty(), 0));
        ret_value = ctx.builder->CreateLoad(ctx.builder->getInt1Ty(), elem_ptr);
        break;
    }
    case VarType::STRING:
    case VarType::STRUCT:
    case VarType::DICT:
    case VarType::LIST: {
        // 引用类型
        auto elem_ptr = ctx.builder->CreateBitCast(
            elem_ptr_ptr,
            llvm::PointerType::get(
                llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0),
                0));
        ret_value = ctx.builder->CreateLoad(
            llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0),
            elem_ptr);
        break;
    }
    default:
        // 对于未知类型，作为整数处理
        auto elem_ptr = ctx.builder->CreateBitCast(
            elem_ptr_ptr, llvm::PointerType::get(ctx.builder->getInt64Ty(), 0));
        ret_value = ctx.builder->CreateLoad(ctx.builder->getInt64Ty(), elem_ptr);
        break;
    }
    
    return 0;
}

int IndexAccess::gencode_dict_access(llvm::Value *dict_val, llvm::Value *key_val, llvm::Value *&ret_value) {
    // 获取字典类型信息
    std::shared_ptr<ASTNode> container_ast;
    container->visit_expr(container_ast);
    auto dict_literal = std::dynamic_pointer_cast<DictLiteral>(container_ast);
    
    std::shared_ptr<ASTNode> index_ast;
    index->visit_expr(index_ast);
    
    // 确定值类型
    VarType value_type = VarType::NONE;
    if (dict_literal) {
        value_type = dict_literal->value_type;
    }
    
    // 根据键类型和值类型确定适当的获取函数
    std::string func_name;
    llvm::FunctionType *get_func_type;
    
    // 检查索引类型
    if (index_ast->type == VarType::STRING) {
        // 字符串键
        switch (value_type) {
        case VarType::INT:
            func_name = "nova_dict_get_str_int";
            break;
        case VarType::FLOAT:
            func_name = "nova_dict_get_str_float";
            break;
        case VarType::BOOL:
            func_name = "nova_dict_get_str_bool";
            break;
        default:
            func_name = "nova_dict_get_str_ptr";
            break;
        }
    } else if (index_ast->type == VarType::INT) {
        // 整数键
        switch (value_type) {
        case VarType::INT:
            func_name = "nova_dict_get_int_int";
            break;
        case VarType::FLOAT:
            func_name = "nova_dict_get_int_float";
            break;
        case VarType::BOOL:
            func_name = "nova_dict_get_int_bool";
            break;
        default:
            func_name = "nova_dict_get_int_ptr";
            break;
        }
    } else {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                     "不支持的字典键类型: " + var_type_to_string(index_ast->type),
                     line, __FILE__, __LINE__);
        return -1;
    }
    
    // 获取或创建函数
    auto get_func = ctx.module->getFunction(func_name);
    if (!get_func) {
        // 创建函数类型，根据不同的函数名
        llvm::Type *ret_type;
        std::vector<llvm::Type*> param_types;
        
        // 字典参数
        param_types.push_back(llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0));
        
        // 键参数
        if (index_ast->type == VarType::STRING) {
            param_types.push_back(llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0));
        } else {
            param_types.push_back(ctx.builder->getInt64Ty());
        }
        
        // 值输出参数
        if (value_type == VarType::INT) {
            param_types.push_back(llvm::PointerType::get(ctx.builder->getInt64Ty(), 0));
        } else if (value_type == VarType::FLOAT) {
            param_types.push_back(llvm::PointerType::get(ctx.builder->getDoubleTy(), 0));
        } else if (value_type == VarType::BOOL) {
            param_types.push_back(llvm::PointerType::get(ctx.builder->getInt1Ty(), 0));
        } else {
            param_types.push_back(llvm::PointerType::get(
                llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0), 0));
        }
        
        // 返回类型是bool
        ret_type = ctx.builder->getInt1Ty();
        
        get_func_type = llvm::FunctionType::get(ret_type, param_types, false);
        get_func = llvm::Function::Create(get_func_type, llvm::Function::ExternalLinkage, func_name, ctx.module.get());
    }
    
    // 创建基本块用于处理键不存在情况
    llvm::Function* curr_func = ctx.builder->GetInsertBlock()->getParent();
    auto normal_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "key_found", curr_func);
    auto error_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "key_notfound", curr_func);
    
    // 分配空间存储结果
    llvm::AllocaInst *result_ptr;
    
    if (value_type == VarType::INT) {
        result_ptr = ctx.builder->CreateAlloca(ctx.builder->getInt64Ty(), nullptr, "result_ptr");
    } else if (value_type == VarType::FLOAT) {
        result_ptr = ctx.builder->CreateAlloca(ctx.builder->getDoubleTy(), nullptr, "result_ptr");
    } else if (value_type == VarType::BOOL) {
        result_ptr = ctx.builder->CreateAlloca(ctx.builder->getInt1Ty(), nullptr, "result_ptr");
    } else {
        result_ptr = ctx.builder->CreateAlloca(
            llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0), 
            nullptr, "result_ptr");
    }
    
    // 调用查找函数
    std::vector<llvm::Value*> args = {dict_val, key_val, result_ptr};
    auto found = ctx.builder->CreateCall(get_func, args, "found");
    
    // 根据结果选择分支
    ctx.builder->CreateCondBr(found, normal_bb, error_bb);
    
    // 键不存在时的处理
    ctx.update_insert_point(error_bb);
    auto err_msg = ctx.builder->CreateGlobalStringPtr("错误: 字典中不存在指定的键", "key_error");
    auto printf_func = ctx.printf_func;
    auto err_fmt = ctx.builder->CreateGlobalStringPtr("%s\n", "err_fmt");
    std::vector<llvm::Value*> err_args = {err_fmt, err_msg};
    ctx.builder->CreateCall(printf_func, err_args);
    
    // 直接调用exit函数退出程序
    auto exit_func = ctx.module->getFunction("exit");
    if (!exit_func) {
        // 如果exit函数未定义，则创建它
        llvm::FunctionType *exit_type = llvm::FunctionType::get(
            ctx.builder->getVoidTy(),
            {ctx.builder->getInt32Ty()},
            false);
        exit_func = llvm::Function::Create(
            exit_type,
            llvm::Function::ExternalLinkage,
            "exit",
            ctx.module.get());
    }
    
    // 调用exit(1)表示错误退出
    ctx.builder->CreateCall(exit_func, {ctx.builder->getInt32(1)});
    
    // 创建一个不可达的终止指令
    ctx.builder->CreateUnreachable();
    
    // 正常情况
    ctx.update_insert_point(normal_bb);
    
    // 加载结果
    ret_value = ctx.builder->CreateLoad(result_ptr->getAllocatedType(), result_ptr, "value");

    return 0;
}