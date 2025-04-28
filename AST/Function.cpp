#include "Function.h"
#include "Common.h"
#include "Return.h"
#include "TypeChecker.h"
#include "ASTParser.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include "StructLiteral.h"

// 辅助函数，从父节点获取类名
std::string get_class_name_from_parent(ASTNode* parent) {
    if (!parent) {
        return "unknown";
    }
    
    if (auto class_node = dynamic_cast<StructLiteral*>(parent)) {
        return class_node->name;
    }
    
    return "unknown";
}

int Function::visit_stmt() {
  if (parent) {
    if (auto class_node = dynamic_cast<StructLiteral*>(parent)) {
      
    }
  }
  
  // // 如果是方法且是第一次处理
  // if (is_method && reference_count == 0) {
  //   // 注册方法信息
  //   add_func_info(name);
  //   auto func_info = lookup_func_info(name);
  //   if (!func_info) {
  //     ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义函数: " + name, line,
  //                   __FILE__, __LINE__);
  //     return -1;
  //   }

  //   // 如果是构造函数，添加特殊标记
  //   if (is_init) {
  //     func_info->get().is_constructor = true;
  //     // 获取父节点的类名
  //     func_info->get().class_name = get_class_name_from_parent(parent);
  //   }

  //   // 设置参数类型
  //   for (auto &param : params) {
  //     func_info->get().param_types.push_back(VarType::NONE);
  //   }
    
  //   result = VarType::NONE;
  //   return 0;
  // }
  
    // std::cout << "function visit_stmt: " << name << " " << line << std::endl;
  // 标准函数处理逻辑
  if (reference_count == 1) {
    // std::cout << "function visit_stmt 1: " << name << " " << line << std::endl;

    // set param type
    for (auto &param : params) {
      if (!param.second) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未推导参数类型: " + param.first, line, __FILE__, __LINE__);
        return -1;
      }
      add_var(param.first, param.second, true);
    }

    // process function body
    for (auto &stmt : body) {
      int ret = stmt->visit_stmt();
      if (ret == -1) {
        return -1;
      }
      auto stmt_node = dynamic_cast<Return*>(stmt.get());
      if (stmt_node) {
        return_ast = stmt_node->value;
        type = stmt_node->type; // last return decide return type
      }
    }

    return 0;
  } else {
    // std::cout << "function visit_stmt 0: " << name << " " << line << std::endl;
    // 首次注册函数
    if (reference_count == 0) {
        add_func(name, shared_from_this());
    }
    return 0;
  }
}

int Function::visit_expr(std::shared_ptr<ASTNode> &self) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "函数定义不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
}

int Function::gencode_stmt() {
    // 判断是否是方法(通过parent)
    bool is_method = parent != nullptr;
  
    // 如果是方法，采用特殊处理
    if (is_method) {
        // 获取父节点的类名
        std::string class_name = get_class_name_from_parent(parent);
      
        // 获取类类型
        auto class_type = llvm::StructType::getTypeByName(*ctx.llvm_context, class_name);
        if (!class_type) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "未找到类: " + class_name, line, __FILE__, __LINE__);
            return -1;
        }
        
        // 创建包含'self'作为第一个参数的函数签名
        std::vector<llvm::Type*> param_types;
        
        // Self参数（指向类结构的指针）
        param_types.push_back(llvm::PointerType::get(class_type, 0));
        
        // 添加其他参数
        for (size_t i = 1; i < params.size(); i++) {
            // 为简化起见，所有参数使用i64
            // 在更完整的实现中，你会跟踪每个参数的类型
            param_types.push_back(llvm::Type::getInt64Ty(*ctx.llvm_context));
        }
        
        // 创建函数类型
        auto return_type = llvm::Type::getVoidTy(*ctx.llvm_context);
        auto function_type = llvm::FunctionType::get(return_type, param_types, false);
        
        // 创建函数
        std::string method_full_name = class_name + "_" + name;
        auto function = llvm::Function::Create(
            function_type, llvm::Function::ExternalLinkage, method_full_name, ctx.module.get());
        
        // 设置参数名称
        size_t idx = 0;
        for (auto &arg : function->args()) {
            if (idx == 0) {
                arg.setName("self");
            } else {
                arg.setName(params[idx].first);
            }
            idx++;
        }
        
        // 将函数存储在上下文中
        ctx.current_function = function;
        
        // 为函数创建基本块
        auto entry_block = llvm::BasicBlock::Create(*ctx.llvm_context, "entry", function);
        ctx.builder->SetInsertPoint(entry_block);
        
        // 生成函数体代码
        for (auto &stmt : body) {
            int ret = stmt->gencode_stmt();
            if (ret == -1) {
                return -1;
            }
        }
        
        // 添加返回指令
        ctx.builder->CreateRetVoid();
        
        return 0;
    }
    
    // 普通函数逻辑
    return 0;
}

int Function::gencode_expr(VarType expected_type, llvm::Value *&value) {
    value = nullptr;
    return 0;
}