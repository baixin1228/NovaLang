#include "Function.h"
#include "Common.h"
#include "Return.h"
#include "TypeChecker.h"
#include "ASTParser.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include "StructLiteral.h"
#include "If.h"
#include "For.h"
#include "While.h"

int Function::visit_stmt() {
  // 标准函数处理逻辑
  if (reference_count == 1) {
    std::cout << "function visit_stmt 1: " << name << " line:" << line << std::endl;

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
    if (return_ast == nullptr) {
      throw std::runtime_error("函数没有返回值: " + name + " code:" + std::to_string(line) + " line:" + std::to_string(__LINE__));
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "函数没有返回值", line, __FILE__, __LINE__);
      return -1;
    }
    return 0;
  } else {
    std::cout << "function visit_stmt 0: " << name << " line:" << line << std::endl;
    // 首次注册函数
    if (reference_count == 0) {
      auto func_name = name;
      if (parent) {
        auto cls = dynamic_cast<StructLiteral*>(parent);
        if (cls->struct_type == StructType::CLASS) {
          func_name = cls->name + "." + name;
        }
      }
      auto func_info = std::make_shared<FuncInfo>();
      func_info->node = shared_from_this();
      add_func(func_name, func_info);
    }
    return 0;
  }
}

int Function::visit_expr(std::shared_ptr<ASTNode> &self) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "函数定义不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
}

void Function::GenLocalVar(Assign &assign) {
  if (!assign.need_create || assign.get_scope_depth() <= 0) {
    return;
  }

  auto var_info = assign.lookup_var(assign.var, assign.line);
  VarType type = var_info->node->type;
  llvm::Type *ty;
  switch (type) {
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
  ptr->setAlignment(llvm::Align(get_type_align(type)));
  var_info->llvm_obj = ptr;
}

int Function::gencode_stmt() {
  std::cout << "==== Generating function: " << name
            << " ====" << std::endl;
  auto func_info = lookup_func(name);
  if (!func_info) {
    throw std::runtime_error("未定义函数: " + name +
                             " code:" + std::to_string(line) +
                             " line:" + std::to_string(__LINE__));
    return -1;
  }

    std::vector<llvm::Type*> llvm_param_types;
    for (auto &type : params) {
      switch (type.second->type) {
      case VarType::INT:
        llvm_param_types.push_back(ctx.builder->getInt64Ty());
        break;
      case VarType::FLOAT:
        llvm_param_types.push_back(ctx.builder->getDoubleTy());
        break;
      case VarType::BOOL:
        llvm_param_types.push_back(ctx.builder->getInt1Ty());
        break;
      default:
        throw std::runtime_error(
            "未知参数类型: " + var_type_to_string(type.second->type) +
              " code:" + std::to_string(line) +
            " line:" + std::to_string(__LINE__));
        return -1;
        break;
      }
    }
    llvm::Type* llvm_return_type;
    switch (type) {
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
      default:
        llvm_return_type = ctx.builder->getVoidTy();
        break;
    }
    auto func_ty = llvm::FunctionType::get(llvm_return_type, llvm_param_types, false);
    auto llvm_func = llvm::Function::Create(func_ty, llvm::Function::ExternalLinkage, name, *ctx.module);
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
        auto type = params[i].second->type;
        if (type == VarType::NONE) {
          throw std::runtime_error("未知参数类型: " + param + " code:" + std::to_string(line) + " line:" + std::to_string(__LINE__));
          return -1;
        }

        llvm::AllocaInst *alloc;
        switch (type) {
          case VarType::INT:
            alloc = ctx.builder->CreateAlloca(ctx.builder->getInt64Ty(), nullptr, param);     
            alloc->setAlignment(llvm::Align(get_type_align(type)));
            break;
          case VarType::FLOAT:
            alloc = ctx.builder->CreateAlloca(ctx.builder->getDoubleTy(), nullptr, param);
            alloc->setAlignment(llvm::Align(get_type_align(type)));
            break;
          case VarType::BOOL:
            alloc = ctx.builder->CreateAlloca(ctx.builder->getInt1Ty(), nullptr, param);
            alloc->setAlignment(llvm::Align(get_type_align(type)));
            break;
          default:
            break;
        }

        auto str = ctx.builder->CreateStore(&arg, alloc);
        str->setAlignment(llvm::Align(get_type_align(type)));
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