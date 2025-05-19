#include "IndexAssign.h"
#include "TypeChecker.h"
#include "ListLiteral.h"
#include "DictLiteral.h"

int IndexAssign::visit_stmt() {
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
    
    // 处理值表达式
    std::shared_ptr<ASTNode> value_ast;
    if (value->visit_expr(value_ast) != 0) {
        return -1;
    }
    
    // 根据容器类型进行类型检查
    if (container_ast->type == VarType::LIST) {
        // 数组索引必须是整数
        if (index_ast->type != VarType::INT) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "Array index must be an integer, got: " + var_type_to_string(index_ast->type),
                         line, __FILE__, __LINE__);
            return -1;
        }
        
        // 检查赋值类型是否与数组元素类型匹配
        auto list_literal = dynamic_cast<ListLiteral*>(container_ast.get());
        if (list_literal && list_literal->element_type != VarType::NONE) {
            if (value_ast->type != list_literal->element_type) {
                ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                             "Cannot assign value of type " + var_type_to_string(value_ast->type) +
                             " to array element of type " + var_type_to_string(list_literal->element_type),
                             line, __FILE__, __LINE__);
                return -1;
            }
        }
    } 
    else if (container_ast->type == VarType::DICT) {
        // 字典可以用不同类型的键，但值类型通常需要一致
        auto dict_literal = dynamic_cast<DictLiteral*>(container_ast.get());
        if (dict_literal && dict_literal->value_type != VarType::NONE) {
            if (value_ast->type != dict_literal->value_type) {
                ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                             "Cannot assign value of type " + var_type_to_string(value_ast->type) +
                             " to dictionary with value type " + var_type_to_string(dict_literal->value_type),
                             line, __FILE__, __LINE__);
                return -1;
            }
        }
    }
    else {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                     "Cannot assign to index of non-container type: " + var_type_to_string(container_ast->type),
                     line, __FILE__, __LINE__);
        return -1;
    }
    
    return 0;
}

int IndexAssign::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
    // 索引赋值通常用作语句，不作为表达式
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                 "Index assignment cannot be used as an expression",
                 line, __FILE__, __LINE__);
    return -1;
}

int IndexAssign::gencode_stmt() {
    llvm::Value *ret_value = nullptr;
    return gencode_expr(VarType::NONE, ret_value);
}

int IndexAssign::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
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
    
    // 获取要赋的值
    llvm::Value *value_val = nullptr;
    if (value->gencode_expr(value->type, value_val) != 0) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                     "无法生成值表达式的代码",
                     line, __FILE__, __LINE__);
        return -1;
    }

    // 确定访问的是列表还是字典
    std::shared_ptr<ASTNode> container_ast;
    if (container->visit_expr(container_ast) != 0) {
        return -1;
    }

    if (container_ast->type == VarType::LIST) {
        // 处理列表索引赋值
        return gencode_list_assign(container_val, index_val, value_val, ret_value);
    }
    else if (container_ast->type == VarType::DICT) {
        // 处理字典索引赋值
        return gencode_dict_assign(container_val, index_val, value_val, ret_value);
    }
    
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                 "不支持的容器类型进行索引赋值: " + var_type_to_string(container_ast->type),
                 line, __FILE__, __LINE__);
    return -1;
}

int IndexAssign::gencode_list_assign(llvm::Value *list_val, llvm::Value *index_val, llvm::Value *value_val, llvm::Value *&ret_value) {
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
    auto end_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "index_end", curr_func);
    
    ctx.builder->CreateCondBr(out_of_bounds, error_bb, normal_bb);
    
    // 处理越界情况
    ctx.update_insert_point(error_bb);
    auto err_msg = ctx.builder->CreateGlobalStringPtr("错误: 数组索引越界", "err_msg");
    auto printf_func = ctx.printf_func;
    auto err_fmt = ctx.builder->CreateGlobalStringPtr("%s\n", "err_fmt");
    std::vector<llvm::Value*> err_args = {err_fmt, err_msg};
    ctx.builder->CreateCall(printf_func, err_args);
    
    // 使用空指针作为错误返回值
    ret_value = nullptr;
    ctx.builder->CreateBr(end_bb);
    
    // 正常赋值
    ctx.update_insert_point(normal_bb);
    
    // 计算元素偏移量 - 开始位置是在元素数量之后
    auto elem_size = ctx.builder->getInt64(8); // 假设每个元素8字节
    auto offset_base = ctx.builder->getInt64(sizeof(size_t));
    auto index_offset = ctx.builder->CreateMul(index_val, elem_size);
    auto offset_val = ctx.builder->CreateAdd(offset_base, index_offset, "elem_offset");
    
    // 获取元素指针
    auto elem_ptr_ptr = ctx.builder->CreateGEP(
        ctx.builder->getInt8Ty(), byte_ptr, offset_val, "elem_ptr_ptr");
    
    VarType element_type = VarType::NONE;
    if (list_literal) {
        element_type = list_literal->element_type;
    }
    
    // 根据元素类型存储值
    switch (element_type) {
    case VarType::INT: {
        auto elem_ptr = ctx.builder->CreateBitCast(
            elem_ptr_ptr, llvm::PointerType::get(ctx.builder->getInt64Ty(), 0));
        ctx.builder->CreateStore(value_val, elem_ptr);
        ret_value = value_val;  // 返回值是赋的值
        break;
    }
    case VarType::FLOAT: {
        auto elem_ptr = ctx.builder->CreateBitCast(
            elem_ptr_ptr, llvm::PointerType::get(ctx.builder->getDoubleTy(), 0));
        ctx.builder->CreateStore(value_val, elem_ptr);
        ret_value = value_val;
        break;
    }
    case VarType::BOOL: {
        auto elem_ptr = ctx.builder->CreateBitCast(
            elem_ptr_ptr, llvm::PointerType::get(ctx.builder->getInt1Ty(), 0));
        ctx.builder->CreateStore(value_val, elem_ptr);
        ret_value = value_val;
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
        ctx.builder->CreateStore(value_val, elem_ptr);
        ret_value = value_val;
        break;
    }
    default:
        // 默认情况下尝试作为指针存储
        auto elem_ptr = ctx.builder->CreateBitCast(
            elem_ptr_ptr,
            llvm::PointerType::get(
                llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0),
                0));
        ctx.builder->CreateStore(value_val, elem_ptr);
        ret_value = value_val;
        break;
    }
    
    ctx.builder->CreateBr(end_bb);
    
    // 设置插入点到结束基本块
    ctx.update_insert_point(end_bb);
    
    return 0;
}

int IndexAssign::gencode_dict_assign(llvm::Value *dict_val, llvm::Value *key_val, llvm::Value *value_val, llvm::Value *&ret_value) {
    // 获取字典类型信息
    std::shared_ptr<ASTNode> container_ast;
    container->visit_expr(container_ast);
    auto dict_literal = std::dynamic_pointer_cast<DictLiteral>(container_ast);
    
    std::shared_ptr<ASTNode> index_ast;
    index->visit_expr(index_ast);
    
    std::shared_ptr<ASTNode> value_ast;
    value->visit_expr(value_ast);
    
    // 确定值类型
    VarType value_type = value_ast->type;
    
    // 根据键类型和值类型确定适当的设置函数
    std::string func_name;
    
    // 检查索引类型
    if (index_ast->type == VarType::STRING) {
        // 字符串键
        switch (value_type) {
        case VarType::INT:
            func_name = "nova_dict_set_str_int";
            break;
        case VarType::FLOAT:
            func_name = "nova_dict_set_str_float";
            break;
        case VarType::BOOL:
            func_name = "nova_dict_set_str_bool";
            break;
        default:
            func_name = "nova_dict_set_str_ptr";
            break;
        }
    } else if (index_ast->type == VarType::INT) {
        // 整数键
        switch (value_type) {
        case VarType::INT:
            func_name = "nova_dict_set_int_int";
            break;
        case VarType::FLOAT:
            func_name = "nova_dict_set_int_float";
            break;
        case VarType::BOOL:
            func_name = "nova_dict_set_int_bool";
            break;
        default:
            func_name = "nova_dict_set_int_ptr";
            break;
        }
    } else {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                     "不支持的字典键类型: " + var_type_to_string(index_ast->type),
                     line, __FILE__, __LINE__);
        return -1;
    }
    
    // 获取或创建函数
    auto set_func = ctx.module->getFunction(func_name);
    if (!set_func) {
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
        
        // 值参数
        if (value_type == VarType::INT) {
            param_types.push_back(ctx.builder->getInt64Ty());
        } else if (value_type == VarType::FLOAT) {
            param_types.push_back(ctx.builder->getDoubleTy());
        } else if (value_type == VarType::BOOL) {
            param_types.push_back(ctx.builder->getInt1Ty());
        } else {
            param_types.push_back(llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0));
        }
        
        // 返回类型是void
        ret_type = ctx.builder->getVoidTy();
        
        auto set_func_type = llvm::FunctionType::get(ret_type, param_types, false);
        set_func = llvm::Function::Create(set_func_type, llvm::Function::ExternalLinkage, func_name, ctx.module.get());
    }
    
    // 调用设置函数
    std::vector<llvm::Value*> args = {dict_val, key_val, value_val};
    ctx.builder->CreateCall(set_func, args);
    
    // 设置返回值为赋的值
    ret_value = value_val;
    
    return 0;
} 