#include "Call.h"
#include "Context.h"
#include "Function.h"
#include "Variable.h"
#include "TypeChecker.h"
#include <execinfo.h>
#include "StructLiteral.h"

int Call::visit_stmt() {
    std::shared_ptr<ASTNode> self_ast;
    return visit_expr(self_ast);
}

int Call::visit_func_expr(std::shared_ptr<ASTNode> &self) {
    // if (forward_expr) {
    //     forward_expr->visit_stmt(result);
    // }

    auto ast_node = lookup_func(name);
    if (!ast_node) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义函数: " + name, line, __FILE__, __LINE__);
        return -1;
    }
    
    auto func_node = dynamic_cast<Function*>(ast_node.get());
    if (!func_node) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "调用非函数类型: " + name,
                    line, __FILE__, __LINE__);
      return -1;
    }

    if (args.size() != func_node->params.size()) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "参数数量不匹配: 期望 " + std::to_string(func_node->params.size()) +
                        ", 得到 " + std::to_string(args.size()),
                    line, __FILE__, __LINE__);
        return -1;
    }

    // update params type
    for (size_t i = 0; i < args.size(); i++) {
        VarType arg_type;
        std::shared_ptr<ASTNode> arg_ast;
        int ret = args[i]->visit_expr(arg_ast);
        if (ret == -1) {
            return -1;
        }
        if (func_node->params[i].second && func_node->params[i].second->type != arg_type) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                        "参数类型不匹配: 期望 " +
                            var_type_to_string(func_node->params[i].second->type) + ", 得到 " +
                            var_type_to_string(arg_type),
                        line, __FILE__, __LINE__);
            return -1;
        }
        func_node->params[i].second = arg_ast;
    }
    auto func = lookup_ast_function(name);
    if (auto func_node = dynamic_cast<Function*>(func)) {
        func_node->reference_count++;
        func_node->visit_stmt();
    } else {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "函数ASTNode查询失败: " + name, line, __FILE__, __LINE__);
        return -1;
    }
    self = func_node->return_ast;
    type = func_node->type;
    return 0;
}

// int Call::visit_class_expr(VarType &result, std::shared_ptr<ASTNode> &self) {
//     auto struct_info = lookup_struct_info(name);

//     auto struct_ast = lookup_ast_struct(name);
//     if (!struct_ast) {
//         ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "结构体ASTNode查询失败: " + name, line, __FILE__, __LINE__);
//         return -1;
//     }
//     auto struct_node = dynamic_cast<StructLiteral*>(struct_ast);

//     auto init_func_info = struct_node->init_method->lookup_func_info(struct_node->init_method->name);
//     if (!init_func_info) {
//         ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "初始化方法查询失败: " + struct_node->init_method->name, line, __FILE__, __LINE__);
//         return -1;
//     }

//     if (args.size() != init_func_info->get().param_types.size()) {
//         ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
//                     "参数数量不匹配: 期望 " + std::to_string(init_func_info->get().param_types.size()) +
//                         ", 得到 " + std::to_string(args.size()),
//                     line, __FILE__, __LINE__);
//         return -1;
//     }

//     // update params type
//     for (size_t i = 0; i < args.size(); i++) {
//         VarType arg_type;
//         std::shared_ptr<ASTNode> arg_ast;
//         int ret = args[i]->visit_expr(arg_type, arg_ast);
//         if (ret == -1) {
//             return -1;
//         }
//         if (init_func_info->get().param_types[i] != VarType::NONE && init_func_info->get().param_types[i] != arg_type) {
//             ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
//                         "参数类型不匹配: 期望 " +
//                             var_type_to_string(init_func_info->get().param_types[i]) + ", 得到 " +
//                             var_type_to_string(arg_type),
//                         line, __FILE__, __LINE__);
//             return -1;
//         }
//         init_func_info->get().param_types[i] = arg_type;
//     }
//     struct_node->reference_count++;
//     int ret = struct_node->visit_stmt(result);
//     if (ret == -1) {
//         return -1;
//     }
//     result = VarType::STRUCT;
//     return 0;
// }

int Call::visit_expr(std::shared_ptr<ASTNode> &self_ast) {
    // auto struct_info = lookup_struct_info(name);
    // if (struct_info) {
    //     return visit_struct_expr(result, self_ast);
    // }
    // std::cout << "call visit_expr: " << name << " " << line << std::endl;
    return visit_func_expr(self_ast);
}

int Call::gencode_stmt() {
    gencode_expr(VarType::NONE);
    return 0;
}

llvm::Value *Call::gencode_expr(VarType expected_type) {
   
    // // 处理对象实例化
    // if (ctx.is_custom_type(name)) {
    //     // 这是一个类的实例化
    //     // 获取类型
    //     auto class_type = llvm::StructType::getTypeByName(*ctx.llvm_context, name);
    //     if (!class_type) {
    //         ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
    //                      "未找到类型: " + name, line, __FILE__, __LINE__);
    //         return nullptr;
    //     }
        
    //     // 为对象分配内存
    //     auto memory_block_type = ctx.runtime_manager->getNovaMemoryBlockType();
    //     auto malloc_func = ctx.runtime_manager->getRuntimeFunction("malloc");
        
    //     // 获取数据布局以计算大小
    //     auto& data_layout = ctx.module->getDataLayout();
        
    //     // 计算类结构的大小
    //     auto size_in_bytes = data_layout.getTypeAllocSize(class_type);
        
    //     std::vector<llvm::Value*> malloc_args = {
    //         ctx.builder->getInt64(size_in_bytes)
    //     };
        
    //     auto memory_block = ctx.builder->CreateCall(malloc_func, malloc_args, "obj_mem");
        
    //     // 转换为类指针
    //     auto class_ptr = ctx.builder->CreateBitCast(
    //         memory_block, 
    //         llvm::PointerType::get(class_type, 0), 
    //         "obj_ptr");
        
    //     // 如果存在，调用构造函数
    //     std::string init_method = name + "_" + "__init__";
    //     auto init_func = ctx.module->getFunction(init_method);
        
    //     if (init_func) {
    //         // 准备构造函数的参数
    //         std::vector<llvm::Value*> ctor_args;
    //         ctor_args.push_back(class_ptr);  // 第一个参数是 'self'
            
    //         // 添加所有其他参数
    //         for (auto &arg : args) {
    //             ctor_args.push_back(arg->gencode_expr(VarType::NONE));
    //         }
            
    //         // 调用构造函数
    //         ctx.builder->CreateCall(init_func, ctor_args);
    //     }
        
    //     return memory_block;  // 返回内存块
    // }
    
    // 普通函数调用
    auto func = lookup_func(name);
    if (!func) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义函数: " + name, line, __FILE__, __LINE__);
        return nullptr;
    }
    auto func_node = dynamic_cast<Function*>(func.get());
    if (!func_node) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "不是函数类型: " + name, line, __FILE__, __LINE__);
        return nullptr;
    }
    std::vector<llvm::Value *> llvm_args;
    for (auto &arg : args) {
        llvm_args.push_back(arg->gencode_expr(VarType::NONE));
    }
    return ctx.builder->CreateCall(func_node->llvm_obj, llvm_args, "call");
}