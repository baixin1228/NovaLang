#include "Function.h"
#include "Annotation.h"
#include "Common.h"
#include "Return.h"
#include "TypeChecker.h"
#include "ASTParser.h"
#include "NoOp.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <memory>
#include "StructLiteral.h"
#include "If.h"
#include "For.h"
#include "While.h"
#include "Void.h"

int Function::visit_stmt() {
  std::string type_str = "function";
  if (parent && parent->type == VarType::CLASS) {
    type_str = "method";
  }
  
  for (auto& annotation : annotations) {
    // 这里可以添加对特定注解的特殊处理
    // 比如 @abstractmethod 可以标记函数需要被子类实现
    if (annotation->name == "abstractmethod") {
      is_abstract = true;
      std::cout << "Function: " << name << " is marked as abstract method [行 " << line << "]\n";
      
      // 检查是否只包含pass语句
      bool only_has_pass = false;
      if (body.size() == 1) {
        auto noop = std::dynamic_pointer_cast<NoOp>(body[0]);
        if (noop) {
          only_has_pass = true;
        }
      }
      
      // 对于抽象方法，如果只包含pass语句，认为是正确的抽象方法实现
      if (only_has_pass) {
        return 0;
      }
    }
  }
  
  // 标准函数处理逻辑
  if (reference_count == 1) {
    std::cout << "function visit_stmt 1: " << name << " type:" << type_str << " line:" << line << std::endl;

    // set param type
    for (auto &param : params) {
      if (!param.second) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未推导参数类型: " + param.first, line, __FILE__, __LINE__);
        return -1;
      }
      std::cout << "==== Generating add param: " << param.first
                << " line:" << param.second->line << "====" << std::endl;

      auto var_info = std::make_shared<VarInfo>();
      var_info->line = line;
      var_info->node = param.second;
      add_var(param.first, var_info, true);
    }

    int last_line = 0;
    // process function body
    for (auto &stmt : body) {
      int ret = stmt->visit_stmt();
      if (ret == -1) {
        return -1;
      }
      auto stmt_node = dynamic_cast<Return*>(stmt.get());
      if (stmt_node) {
        return_ast = stmt_node->value;
      }
      last_line = stmt->line;
    }
    
    // 检查是否有返回值，对于抽象方法可以没有返回值（只包含pass语句）
    if (return_ast == nullptr && !is_abstract) {
      if (ctx.strict_mode) {
        throw std::runtime_error("函数没有返回值: " + name + " code:" + 
        std::to_string(line) + " file:" + __FILE__ + " line:" + std::to_string(__LINE__));
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "函数没有返回值", line, __FILE__, __LINE__);
        return -1;
      } else {
        auto void_node = std::make_shared<Void>(ctx, last_line);
        auto return_node = std::make_shared<Return>(ctx, void_node, last_line);
        body.push_back(return_node);
        int ret = return_node->visit_stmt();
        if (ret == -1) {
          return -1;
        }
        return_ast = void_node;
      }
    }
    return 0;
  } else {
    std::cout << "function visit_stmt 0: " << name << " type:" << type_str << " line:" << line << std::endl;
    // 首次注册函数
    if (reference_count == 0) {
      auto func_info = std::make_shared<FuncInfo>();
      func_info->node = shared_from_this();
      add_func(name, func_info);
    }
    return 0;
  }
}

int Function::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
    print_backtrace();
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "函数定义不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
}

void Function::GenLocalVar(Assign &assign) {
  if (!assign.need_create || assign.get_scope_depth() <= 0) {
    return;
  }

  auto var_info = assign.lookup_var(assign.var, assign.line);
  VarType var_type = var_info->node->type;
  llvm::Type *ty;
  switch (var_type) {
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
  auto ptr = ctx.builder->CreateAlloca(ty, nullptr, assign.var);
  ptr->setAlignment(llvm::Align(get_type_align(var_type)));
  var_info->llvm_obj = ptr;
}

int Function::gencode_stmt() {
  std::cout << "==== Generating function: " << name
            << " ====" << std::endl;
  auto func_info = lookup_func(name);
  if (!func_info) {
    throw std::runtime_error("未定义函数: " + name +
                             " code:" + std::to_string(line) +
                             " file:" + __FILE__ +
                             " line:" + std::to_string(__LINE__));
    return -1;
  }

    std::vector<llvm::Type*> llvm_param_types;
    for (auto &p_type : params) {
      switch (p_type.second->type) {
      case VarType::INT:
        llvm_param_types.push_back(ctx.builder->getInt64Ty());
        break;
      case VarType::FLOAT:
        llvm_param_types.push_back(ctx.builder->getDoubleTy());
        break;
      case VarType::BOOL:
        llvm_param_types.push_back(ctx.builder->getInt1Ty());
        break;
      case VarType::INSTANCE:
        llvm_param_types.push_back(llvm::PointerType::get(
            ctx.runtime_manager->getNovaMemoryBlockType(), 0));
        break;
      default:
        throw std::runtime_error(
            "未知参数类型: " + var_type_to_string(p_type.second->type) +
            " code:" + std::to_string(line) +
            " line:" + std::to_string(__LINE__));
        return -1;
        break;
      }
    }
    llvm::Type* llvm_return_type;
    switch (return_ast->type) {
      case VarType::INT:
        llvm_return_type = ctx.builder->getInt64Ty();
        break;
      case VarType::FLOAT:
        llvm_return_type = ctx.builder->getDoubleTy();
        break;
      case VarType::BOOL:
        llvm_return_type = ctx.builder->getInt1Ty();
        break;
      case VarType::STRUCT:
        llvm_return_type = llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0);
        break;
      case VarType::VOID:
        llvm_return_type = ctx.builder->getVoidTy();
        break;
      default:
        throw std::runtime_error("未知返回类型: " + var_type_to_string(return_ast->type) +
              " code:" + std::to_string(line) +
            " line:" + std::to_string(__LINE__));
        return -1;
        break;
    }
    auto func_ty = llvm::FunctionType::get(llvm_return_type, llvm_param_types, false);
    std::string func_name = name;
    if (parent && parent->type == VarType::CLASS) {
      auto class_node = dynamic_cast<StructLiteral *>(parent);
      func_name = class_node->name + "." + name;
    }
    auto llvm_func = llvm::Function::Create(func_ty, llvm::Function::ExternalLinkage, func_name, *ctx.module);
    func_info->llvm_func = llvm_func;
    llvm_type = func_ty;

    // 只有在需要生成调试信息时才创建调试信息
    // if (generate_debug_info) {
    //     auto dbg_func = dbg_builder->createFunction(
    //         dbg_file,
    //         func.name,
    //         func.name,
    //         dbg_file,
    //         func.line,
    //         dbg_builder->createSubroutineType(dbg_builder->getOrCreateTypeArray({})),
    //         func.line,
    //         llvm::DINode::FlagPrototyped,
    //         llvm::DISubprogram::SPFlagDefinition
    //     );
    //     llvm_func->setSubprogram(dbg_func);

  //     // 添加调试位置
  //     auto debug_loc =
  //         llvm::DILocation::get(*ctx.llvm_context, func.line, 0, dbg_func);
  //     ctx.builder->SetCurrentDebugLocation(debug_loc);
  // }

  auto block = llvm::BasicBlock::Create(*ctx.llvm_context, "entry", llvm_func);
  ctx.builder->SetInsertPoint(block);
  ctx.current_function = llvm_func;

  size_t i = 0;
  for (auto& arg : llvm_func->args()) {
      auto& param = params[i].first;
      auto param_type = params[i].second->type;
      if (param_type == VarType::NONE) {
        throw std::runtime_error("未知参数类型: " + param + " code:" + std::to_string(line) + " line:" + std::to_string(__LINE__));
        return -1;
      }

      llvm::AllocaInst *alloc;
      switch (param_type) {
        case VarType::INT:
          alloc = ctx.builder->CreateAlloca(ctx.builder->getInt64Ty(), nullptr, param);     
          alloc->setAlignment(llvm::Align(get_type_align(param_type)));
          break;
        case VarType::FLOAT:
          alloc = ctx.builder->CreateAlloca(ctx.builder->getDoubleTy(), nullptr, param);
          alloc->setAlignment(llvm::Align(get_type_align(param_type)));
          break;
        case VarType::BOOL:
          alloc = ctx.builder->CreateAlloca(ctx.builder->getInt1Ty(), nullptr, param);
          alloc->setAlignment(llvm::Align(get_type_align(param_type)));
          break;
        case VarType::INSTANCE:
          alloc = ctx.builder->CreateAlloca(
              llvm::PointerType::get(
                  ctx.runtime_manager->getNovaMemoryBlockType(), 0),
              nullptr, param);
          alloc->setAlignment(llvm::Align(get_type_align(param_type)));
          break;
        default:
          throw std::runtime_error("未知参数类型: " + param + " code:" + std::to_string(line) + " line:" + std::to_string(__LINE__));
          return -1;
          break;
      }

      auto str = ctx.builder->CreateStore(&arg, alloc);
      str->setAlignment(llvm::Align(get_type_align(param_type)));
      auto param_info = lookup_var(param, line);
      if (!param_info) {
        throw std::runtime_error("未定义参数: " + param + " code:" + std::to_string(line) + " line:" + std::to_string(__LINE__));
        return -1;
      }
      std::cout << "==== Generating params variable: " << param
                << " line:" << params[i].second->line << " ====" << std::endl;
      param_info->llvm_obj = alloc;
      ++i;
  }

  for (auto& stmt : body) {
    if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
      assign->gencode_var();
    }
    if (auto *iff = dynamic_cast<If *>(stmt.get())) {
      for (auto &stmt : iff->body) {
        if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
          assign->gencode_var();
        }
      }
    }
    if (auto *ffor = dynamic_cast<For *>(stmt.get())) {
      for (auto &stmt : ffor->body) {
        if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
          assign->gencode_var();
        }
      }
    }
    if (auto *whl = dynamic_cast<While *>(stmt.get())) {
      for (auto &stmt : whl->body) {
        if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
          assign->gencode_var();
        }
      }
    }
    if (auto *func = dynamic_cast<Function *>(stmt.get())) {
      if (func->gencode_stmt() == -1) {
        return -1;
      }
    }
  }

  for (auto& stmt : body) {
      if (stmt->gencode_stmt() == -1) {
        return -1;
    }
  }

  for (auto& param : params) {
      // func.erase_llvm_symbol(param);
  }
  std::cout << "Generated function: " << name << std::endl;
  return 0;
}

int Function::gencode_expr(VarType expected_type, llvm::Value *&value) {
    value = nullptr;
    return 0;
}