#include "CodeGen.h"
#include "Function.h"
#include "Assign.h"
#include "If.h"
#include "For.h"
#include "While.h"

int CodeGen::generate_function(Function &func) {
  std::cout << "==== Generating function: " << func.name
            << " ====" << std::endl;
  if (!ctx.has_func(func.name)) {
    throw std::runtime_error("未定义函数: " + func.name +
                             " code:" + std::to_string(func.line) +
                             " line:" + std::to_string(__LINE__));
    return -1;
  }
    auto &[param_types, return_type] = ctx.get_func_type(func.name);

    std::vector<llvm::Type*> llvm_param_types;
    for (auto& type : param_types) {
        switch (type) {
          case VarType::INT:
            llvm_param_types.push_back(builder.getInt64Ty());
            break;
          case VarType::FLOAT:
            llvm_param_types.push_back(builder.getDoubleTy());
            break;
          case VarType::BOOL:
            llvm_param_types.push_back(builder.getInt1Ty());
            break;
          default:
            throw std::runtime_error("未知参数类型: " + std::to_string(static_cast<int>(type)) + " code:" + std::to_string(func.line) + " line:" + std::to_string(__LINE__));
            return -1;
            break;
        }
    }
    llvm::Type* llvm_return_type;
    switch (return_type) {
      case VarType::INT:
        llvm_return_type = builder.getInt64Ty();
        break;
      case VarType::FLOAT:
        llvm_return_type = builder.getDoubleTy();
        break;
      case VarType::BOOL:
        llvm_return_type = builder.getInt1Ty();
        break;
      default:
        llvm_return_type = builder.getVoidTy();
        break;
    }
    auto func_ty = llvm::FunctionType::get(llvm_return_type, llvm_param_types, false);
    auto llvm_func = llvm::Function::Create(func_ty, llvm::Function::ExternalLinkage, func.name, *module);
    functions[func.name] = llvm_func;

    // 只有在需要生成调试信息时才创建调试信息
    if (generate_debug_info) {
        auto dbg_func = dbg_builder->createFunction(
            dbg_file,
            func.name,
            func.name,
            dbg_file,
            func.line,
            dbg_builder->createSubroutineType(dbg_builder->getOrCreateTypeArray({})),
            func.line,
            llvm::DINode::FlagPrototyped,
            llvm::DISubprogram::SPFlagDefinition
        );
        llvm_func->setSubprogram(dbg_func);

        // 添加调试位置
        auto debug_loc = llvm::DILocation::get(context, func.line, 0, dbg_func);
        builder.SetCurrentDebugLocation(debug_loc);
    }

    auto block = llvm::BasicBlock::Create(context, "entry", llvm_func);
    builder.SetInsertPoint(block);
    current_function = llvm_func;

    size_t i = 0;
    for (auto& arg : llvm_func->args()) {
        auto& param = func.params[i];
        auto type = param_types[i];
        if (type == VarType::NONE) {
          throw std::runtime_error("未知参数类型: " + param + " code:" + std::to_string(func.line) + " line:" + std::to_string(__LINE__));
          return -1;
        }

        llvm::AllocaInst *alloc;
        switch (type) {
          case VarType::INT:
            alloc = builder.CreateAlloca(builder.getInt64Ty(), nullptr, param);     
            alloc->setAlignment(llvm::Align(get_type_align(type)));
            break;
          case VarType::FLOAT:
            alloc = builder.CreateAlloca(builder.getDoubleTy(), nullptr, param);
            alloc->setAlignment(llvm::Align(get_type_align(type)));
            break;
          case VarType::BOOL:
            alloc = builder.CreateAlloca(builder.getInt1Ty(), nullptr, param);
            alloc->setAlignment(llvm::Align(get_type_align(type)));
            break;
          default:
            break;
        }

        auto str = builder.CreateStore(&arg, alloc);
        str->setAlignment(llvm::Align(get_type_align(type)));
        func.add_llvm_symbol(param, alloc);
        ++i;
    }

    for (auto& stmt : func.body) {
      if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
        GenLocalVar(*assign);
      }
      if (auto *iff = dynamic_cast<If *>(stmt.get())) {
        for (auto &stmt : iff->body) {
          if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
            GenLocalVar(*assign);
          }
        }
      }
      if (auto *ffor = dynamic_cast<For *>(stmt.get())) {
        for (auto &stmt : ffor->body) {
          if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
            GenLocalVar(*assign);
          }
        }
      }
      if (auto *whl = dynamic_cast<While *>(stmt.get())) {
        for (auto &stmt : whl->body) {
          if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
            GenLocalVar(*assign);
          }
        }
      }
      if (auto *func = dynamic_cast<Function *>(stmt.get())) {
        if (generate_function(*func) == -1) {
          return -1;
        }
      }
    }

    for (auto& stmt : func.body) {
        if (visit_stmt(*stmt) == -1) {
          return -1;
        }
    }

    for (auto& param : func.params) {
        // func.erase_llvm_symbol(param);
    }
    std::cout << "Generated function: " << func.name << std::endl;
    return 0;
} 