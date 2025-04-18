#include <cstdint>
#include <dlfcn.h>
#include <iostream>
#include "ASTNode.h"
#include "Assign.h"
#include "BinOp.h"
#include "BoolLiteral.h"
#include "Call.h"
#include "CodeGen.h"
#include "Common.h"
#include "Context.h"
#include "FloatLiteral.h"
#include "For.h"
#include "Function.h"
#include "Global.h"
#include "If.h"
#include "Increment.h"
#include "IntLiteral.h"
#include "Print.h"
#include "Return.h"
#include "UnaryOp.h"
#include "Variable.h"
#include "While.h"

CodeGen::CodeGen(Context& ctx, bool debug) 
    : context(), 
      module(std::make_unique<llvm::Module>("novalang", context)),
      builder(context),
      ctx(ctx),
      current_function(nullptr),
      printf_func(nullptr),
      dbg_builder(nullptr),
      dbg_file(nullptr),
      dbg_compile_unit(nullptr),
      generate_debug_info(debug) {
    
    // 只有在需要生成调试信息时才初始化调试信息
    if (generate_debug_info) {
        dbg_builder = std::make_unique<llvm::DIBuilder>(*module);
        source_filename = ctx.get_source_filename();
        dbg_file = dbg_builder->createFile(source_filename, ".");
        dbg_compile_unit = dbg_builder->createCompileUnit(
            llvm::dwarf::DW_LANG_C, 
            dbg_file, 
            "NovaLang Compiler", 
            false, 
            "", 
            0
        );
    }

    // 初始化 printf 函数
    auto printf_ty = llvm::FunctionType::get(
        builder.getInt32Ty(),
        {builder.getInt8PtrTy()},
        true
    );
    printf_func = llvm::Function::Create(
        printf_ty,
        llvm::Function::ExternalLinkage,
        "printf",
        *module
    );
}

int CodeGen::generate() {
    auto &stmts = ctx.get_ast();
    // 然后生成所有函数的定义
    for (auto &stmt : stmts) {
      if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
        if (generate_global_variable(*assign) == -1) {
          return -1;
        }
      }
      if (auto *iff = dynamic_cast<If *>(stmt.get())) {
        for (auto &stmt : iff->body) {
          if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
            if (generate_global_variable(*assign) == -1) {
              return -1;
            }
          }
        }
      }
      if (auto *ffor = dynamic_cast<For *>(stmt.get())) {
        for (auto &stmt : ffor->body) {
          if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
            if (generate_global_variable(*assign) == -1) {
              return -1;
            }
          }
        }
      }
      if (auto *whl = dynamic_cast<While *>(stmt.get())) {
        for (auto &stmt : whl->body) {
          if (auto *assign = dynamic_cast<Assign *>(stmt.get())) {
            if (generate_global_variable(*assign) == -1) {
              return -1;
            }
          }
        }
      }
      if (auto *func = dynamic_cast<Function *>(stmt.get())) {
        if (generate_function(*func) == -1) {
          return -1;
        }
      }
    }

    // 生成 main 函数
    auto func_ty = llvm::FunctionType::get(builder.getInt32Ty(), {}, false);
    auto main_func = llvm::Function::Create(func_ty, llvm::Function::ExternalLinkage, "main", *module);
    auto block = llvm::BasicBlock::Create(context, "entry", main_func);
    // 从现在开始所有生成的指令都要放入这个基本块中
    builder.SetInsertPoint(block);
    current_function = main_func;

    // 只有在需要生成调试信息时才创建调试信息
    if (generate_debug_info) {
        auto dbg_func = dbg_builder->createFunction(
            dbg_file,
            "main",
            "main",
            dbg_file,
            0,
            dbg_builder->createSubroutineType(dbg_builder->getOrCreateTypeArray({})),
            0,
            llvm::DINode::FlagPrototyped,
            llvm::DISubprogram::SPFlagDefinition
        );
        main_func->setSubprogram(dbg_func);
    }

    for (auto& stmt : stmts) {
        if (!dynamic_cast<Function*>(stmt.get())) {
            if (visit_stmt(*stmt) == -1) {
              return -1;
            }
        }
    }

    builder.CreateRet(builder.getInt32(0));
    return 0;
}

void CodeGen::print_ir() {
    std::cout << "[IR]\n";
    module->print(llvm::outs(), nullptr);
}

int CodeGen::generate_global_variable(Assign &assign) {
  VarType type = assign.lookup_var_type(assign.var);
  if (type == VarType::NONE) {
    throw std::runtime_error("未定义的变量: " + assign.var + " code:" + std::to_string(assign.line) + " line:" + std::to_string(__LINE__));
    return -1;
  }

  if (assign.need_create) {
    llvm::Type *ty;
    switch (type) {
    case VarType::INT:
      ty = builder.getInt64Ty();
      break;
    case VarType::FLOAT:
      ty = builder.getDoubleTy();
      break;
    case VarType::BOOL:
      ty = builder.getInt1Ty();
      break;
    default:
      ty = builder.getVoidTy();
      break;
    }

    std::cout << "==== Generating global variable: " << assign.var
              << "====" << std::endl;
    // 创建全局变量
    auto global_var = new llvm::GlobalVariable(
        *module,
        ty,
        false, // 是否为常量
        llvm::GlobalValue::ExternalLinkage,
        llvm::Constant::getNullValue(ty), // 初始值
        assign.var
    );
    global_var->setAlignment(llvm::Align(get_type_align(type)));

    assign.add_llvm_symbol(assign.var, global_var);
  }
  return 0;
}

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

int CodeGen::visit_stmt(ASTNode& node) {
    // Set debug location for each statement only if debug info is enabled
    if (generate_debug_info) {
        auto debug_loc = llvm::DILocation::get(context, node.line, 0, current_function->getSubprogram());
        builder.SetCurrentDebugLocation(debug_loc);
    }

    if (auto* assign = dynamic_cast<Assign*>(&node)) {
        VarType type = assign->lookup_var_type(assign->var);
        if (type == VarType::NONE) {
          throw std::runtime_error("未定义的变量: " + assign->var + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
        }

        // GenLocalVar(*assign);
        auto ptr = assign->lookup_llvm_symbol(assign->var);
        if (!ptr) {
          throw std::runtime_error("未定义的变量: " + assign->var +
                                   " code:" + std::to_string(node.line) +
                                   " file:" + std::string(__FILE__) +
                                   " line:" + std::to_string(__LINE__));
        }
        auto value = visit_expr(*assign->value, type);
        auto str = builder.CreateStore(value, ptr);
        str->setAlignment(llvm::Align(get_type_align(type)));
    } else if (auto *inc = dynamic_cast<Increment *>(&node)) {
      auto ptr = inc->lookup_llvm_symbol(inc->var);
      VarType type = inc->lookup_var_type(inc->var);
      if (type == VarType::NONE) {
        throw std::runtime_error("未定义的变量: " + inc->var + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
      }
      auto old_val = builder.CreateLoad(builder.getInt64Ty(), ptr, inc->var + "_old");
      old_val->setAlignment(llvm::Align(get_type_align(type)));
      auto new_val = builder.CreateAdd(old_val, builder.getInt64(1), inc->var + "_inc");
      auto str = builder.CreateStore(new_val, ptr);
      str->setAlignment(llvm::Align(get_type_align(type)));
    } else if (auto *whl = dynamic_cast<While *>(&node)) {
      auto loop_bb =
          llvm::BasicBlock::Create(context, "loop", current_function);
      auto body_bb =
          llvm::BasicBlock::Create(context, "body", current_function);
      auto exit_bb =
          llvm::BasicBlock::Create(context, "exit", current_function);

      builder.CreateBr(loop_bb);
      builder.SetInsertPoint(loop_bb);
      auto cond = visit_expr(*whl->condition);
      builder.CreateCondBr(cond, body_bb, exit_bb);

      builder.SetInsertPoint(body_bb);
      for (auto &stmt : whl->body) {
        if (visit_stmt(*stmt) == -1) {
          return -1;
        }
      }
      builder.CreateBr(loop_bb);

      builder.SetInsertPoint(exit_bb);
    } else if (auto *fr = dynamic_cast<For *>(&node)) {
      auto iter_ptr = builder.CreateAlloca(builder.getInt64Ty(), nullptr, fr->iterator);
      iter_ptr->setAlignment(llvm::Align(get_type_align(VarType::INT)));
      fr->add_llvm_symbol(fr->iterator, iter_ptr);
      auto str = builder.CreateStore(builder.getInt64(0), iter_ptr);
      str->setAlignment(llvm::Align(get_type_align(VarType::INT)));

      auto loop_bb = llvm::BasicBlock::Create(context, "loop", current_function);
      auto body_bb = llvm::BasicBlock::Create(context, "body", current_function);
      auto exit_bb = llvm::BasicBlock::Create(context, "exit", current_function);

      builder.CreateBr(loop_bb);
      builder.SetInsertPoint(loop_bb);
      auto iter_val = builder.CreateLoad(builder.getInt64Ty(), iter_ptr, fr->iterator + "_val");
      iter_val->setAlignment(llvm::Align(get_type_align(VarType::INT)));
      auto end_val = visit_expr(*fr->end);
      auto cond = builder.CreateICmpSLT(iter_val, end_val, "cmp");
      builder.CreateCondBr(cond, body_bb, exit_bb);

      builder.SetInsertPoint(body_bb);
      for (auto &stmt : fr->body) {
        if (visit_stmt(*stmt) == -1) {
          return -1;
        }
      }
      auto iter_new = builder.CreateAdd(iter_val, builder.getInt64(1), fr->iterator + "_inc");
      auto str2 = builder.CreateStore(iter_new, iter_ptr);
      str2->setAlignment(llvm::Align(get_type_align(VarType::INT)));
      builder.CreateBr(loop_bb);

      builder.SetInsertPoint(exit_bb);
      // fr->erase_llvm_symbol(fr->iterator);
    } else if (auto *iff = dynamic_cast<If *>(&node)) {
      std::vector<llvm::BasicBlock *> elif_bbs;
      std::vector<llvm::BasicBlock *> body_bbs;
      llvm::BasicBlock *else_bb = nullptr;
      llvm::BasicBlock *end_bb =
          llvm::BasicBlock::Create(context, "end", current_function);

      // If 分支
      auto then_bb =
          llvm::BasicBlock::Create(context, "then", current_function);
      auto next_bb = iff->elifs.empty() && iff->else_body.empty() ? end_bb
                     : iff->elifs.empty()
                         ? (else_bb = llvm::BasicBlock::Create(
                                context, "else", current_function))
                         : (elif_bbs.push_back(llvm::BasicBlock::Create(
                                context, "elif1", current_function)),
                            elif_bbs.back());
      auto cond = visit_expr(*iff->condition);
      builder.CreateCondBr(cond, then_bb, next_bb);

      builder.SetInsertPoint(then_bb);
      for (auto &stmt : iff->body) {
        if (visit_stmt(*stmt) == -1) {
          return -1;
        }
      }
      builder.CreateBr(end_bb);

      // Elif 分支
      for (size_t i = 0; i < iff->elifs.size(); ++i) {
        builder.SetInsertPoint(elif_bbs[i]);
        auto elif_cond = visit_expr(*iff->elifs[i].first);
        auto elif_body_bb = llvm::BasicBlock::Create(
            context, "elif_body" + std::to_string(i + 1), current_function);
        auto next_elif_bb = i + 1 < iff->elifs.size()
                                ? (elif_bbs.push_back(llvm::BasicBlock::Create(
                                       context, "elif" + std::to_string(i + 2),
                                       current_function)),
                                   elif_bbs.back())
                            : iff->else_body.empty()
                                ? end_bb
                                : (else_bb = llvm::BasicBlock::Create(
                                       context, "else", current_function));
        builder.CreateCondBr(elif_cond, elif_body_bb, next_elif_bb);

        builder.SetInsertPoint(elif_body_bb);
        for (auto &stmt : iff->elifs[i].second) {
          if (visit_stmt(*stmt) == -1) {
            return -1;
          }
        }
        builder.CreateBr(end_bb);
      }

      // Else 分支
      if (!iff->else_body.empty()) {
        builder.SetInsertPoint(else_bb);
        for (auto &stmt : iff->else_body) {
          visit_stmt(*stmt);
        }
        builder.CreateBr(end_bb);
      }

      builder.SetInsertPoint(end_bb);
    } else if (auto *ret = dynamic_cast<Return *>(&node)) {
      auto value = visit_expr(*ret->value);
      builder.CreateRet(value);
    } else if (auto *prt = dynamic_cast<Print *>(&node)) {
      auto value = visit_expr(*prt->value);
      VarType type = visit_expr_type(*prt->value);
      auto fmt =
          type == VarType::INT     ? builder.CreateGlobalStringPtr("%ld\n", "fmt_int")
          : type == VarType::FLOAT ? builder.CreateGlobalStringPtr("%.18lf\n", "fmt_float")
                            : builder.CreateGlobalStringPtr("%d\n", "fmt_bool");
      std::vector<llvm::Value *> args = {fmt};
      if (type == VarType::INT) {
        args.push_back(value);
      } else if (type == VarType::FLOAT) {
        args.push_back(value);
      } else { // bool
        args.push_back(value);
      }
      builder.CreateCall(printf_func, args);
    } else if (auto *call = dynamic_cast<Call *>(&node)) {
      visit_expr(*call);
    } else if (auto *call = dynamic_cast<Global *>(&node)) {
    } else {
      throw std::runtime_error("未知语句 code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
      return -1;
    }
    return 0;
}

void CodeGen::GenLocalVar(Assign& assign) {
    if (!assign.need_create || assign.get_scope_depth() <= 0) {
        return;
    }
    
    VarType type = assign.lookup_var_type(assign.var);
    llvm::Type *ty;
    switch (type) {
        case VarType::INT:
            ty = builder.getInt64Ty();
            break;
        case VarType::FLOAT:
            ty = builder.getDoubleTy();
            break;
        case VarType::BOOL:
            ty = builder.getInt1Ty();
            break;
        default:
            ty = builder.getVoidTy();
            break;
    }
    auto ptr = builder.CreateAlloca(ty, nullptr, assign.var);
    ptr->setAlignment(llvm::Align(get_type_align(type)));
    assign.add_llvm_symbol(assign.var, ptr);
}

llvm::Value* CodeGen::visit_expr(ASTNode& node, VarType expected_type) {
    if (auto* int_lit = dynamic_cast<IntLiteral*>(&node)) {
      if (expected_type != VarType::NONE && expected_type == VarType::FLOAT)
      {
        // 隐式类型转换
        return builder.CreateSIToFP(builder.getInt64(int_lit->value),
                                    builder.getDoubleTy());
      }
      return builder.getInt64(int_lit->value);
    }
    if (auto* float_lit = dynamic_cast<FloatLiteral*>(&node)) {
        return llvm::ConstantFP::get(builder.getDoubleTy(), float_lit->value);
    }
    if (auto* bool_lit = dynamic_cast<BoolLiteral*>(&node)) {
        return builder.getInt1(bool_lit->value);
    }
    if (auto* var = dynamic_cast<Variable*>(&node)) {
        auto ptr = var->lookup_llvm_symbol(var->name);
        if (!ptr) {
            throw std::runtime_error("未定义的变量: " + var->name + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
            return nullptr;
        }

        VarType type = var->lookup_var_type(var->name);
        switch (type) {
            case VarType::INT: {
                auto load = builder.CreateLoad(builder.getInt64Ty(), ptr, var->name + "_load");
                load->setAlignment(llvm::Align(get_type_align(type)));
                if (expected_type != VarType::NONE &&
                    expected_type == VarType::FLOAT) {
                    //隐式类型转换
                  return builder.CreateSIToFP(load, builder.getDoubleTy());
                }
                return load;
            }
            case VarType::FLOAT: {
                auto load = builder.CreateLoad(builder.getDoubleTy(), ptr, var->name + "_load");
                load->setAlignment(llvm::Align(get_type_align(type)));
                return load;
            }
            case VarType::BOOL: {
                auto load = builder.CreateLoad(builder.getInt1Ty(), ptr, var->name + "_load");
                load->setAlignment(llvm::Align(get_type_align(type)));
                return load;
            }
            default:
                throw std::runtime_error("未知变量类型: " + var->name + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
                return nullptr;
        }
    }
    if (auto* binop = dynamic_cast<BinOp*>(&node)) {
      VarType type = visit_expr_type(*binop->left);
      VarType left_expected_type = VarType::NONE;
      VarType right_expected_type = VarType::NONE;
      auto left_type = visit_expr_type(*binop->left);
      auto right_type = visit_expr_type(*binop->right);
      if (expected_type != VarType::NONE) {
        left_expected_type = expected_type;
        right_expected_type = expected_type;
        type = expected_type;
      }
      if (left_type != right_type) {
        if (left_type == VarType::FLOAT) {
          right_expected_type = VarType::FLOAT;
        }
        if (right_type == VarType::FLOAT) {
          left_expected_type = VarType::FLOAT;
        }
        type = VarType::FLOAT;
      }
      if (binop->op == "/") {
        left_expected_type = VarType::FLOAT;
        right_expected_type = VarType::FLOAT;
        type = VarType::FLOAT;
      }
      auto left = visit_expr(*binop->left, left_expected_type);
      auto right = visit_expr(*binop->right, right_expected_type);
        if (left == nullptr || right == nullptr) {
        throw std::runtime_error(
            "未知表达式 code:" + std::to_string(node.line) +
            " line:" + std::to_string(__LINE__));
        return nullptr;
        }
        if (binop->op == "+") {
            return type == VarType::INT ?
                builder.CreateAdd(left, right, "add") :
                builder.CreateFAdd(left, right, "fadd");
        }
        if (binop->op == "-") {
            return type == VarType::INT ?
                builder.CreateSub(left, right, "sub") :
                builder.CreateFSub(left, right, "fsub");
        }
        if (binop->op == "*") {
            return type == VarType::INT ?
                builder.CreateMul(left, right, "mul") :
                builder.CreateFMul(left, right, "fmul");
        }
        if (binop->op == "/") {
            return builder.CreateFDiv(left, right, "fdiv");
        }
        if (binop->op == "//") {
          return builder.CreateSDiv(left, right, "sdiv");
        }
        if (binop->op == "==") {
            if (type == VarType::INT) {
                return builder.CreateICmpEQ(left, right, "eq");
            } else if (type == VarType::FLOAT) {
                return builder.CreateFCmpOEQ(left, right, "feq");
            } else {
                return builder.CreateICmpEQ(left, right, "eq");
            }
        }
        if (binop->op == "<") {
            return type == VarType::INT ?
                builder.CreateICmpSLT(left, right, "cmp") :
                builder.CreateFCmpOLT(left, right, "fcmp");
        }
        if (binop->op == ">") {
            return type == VarType::INT ?
                builder.CreateICmpSGT(left, right, "cmp") :
                builder.CreateFCmpOGT(left, right, "fcmp");
        }
        if (binop->op == ">=") {
            return type == VarType::INT ?
                builder.CreateICmpSGE(left, right, "cmp") :
                builder.CreateFCmpOGE(left, right, "fcmp");
        }
        if (binop->op == "and") {
            return builder.CreateAnd(left, right, "and");
        }
        if (binop->op == "or") {
            return builder.CreateOr(left, right, "or");
        }
        throw std::runtime_error("未知运算符: " + binop->op + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
        return nullptr;
    }
    if (auto* unary = dynamic_cast<UnaryOp*>(&node)) {
        auto expr = visit_expr(*unary->expr);
        if (unary->op == "not") {
            return builder.CreateNot(expr, "not");
        }
        throw std::runtime_error("未知一元运算符: " + unary->op + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
        return nullptr;
    }
    if (auto* call = dynamic_cast<Call*>(&node)) {
        auto func = functions[call->name];
        std::vector<llvm::Value*> args;
        for (auto& arg : call->args) {
            args.push_back(visit_expr(*arg));
        }
        return builder.CreateCall(func, args, "call");
    }
    throw std::runtime_error("未知表达式 code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
    return nullptr;
}

VarType CodeGen::visit_expr_type(ASTNode& node) {
    if (auto* int_lit = dynamic_cast<IntLiteral*>(&node)) {
        return VarType::INT;
    }
    if (auto* float_lit = dynamic_cast<FloatLiteral*>(&node)) {
        return VarType::FLOAT;
    }
    if (auto* bool_lit = dynamic_cast<BoolLiteral*>(&node)) {
        return VarType::BOOL;
    }
    if (auto* var = dynamic_cast<Variable*>(&node)) {
      VarType type = var->lookup_var_type(var->name);
      if (type == VarType::NONE) {
        throw std::runtime_error("未定义的变量: " + var->name + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
        return VarType::NONE;
        }
        return type;
    }
    if (auto* binop = dynamic_cast<BinOp*>(&node)) {
      VarType left_type = visit_expr_type(*binop->left);
      if (binop->op == "==" || binop->op == "<" || binop->op == ">" ||
          binop->op == ">=") {
        return VarType::BOOL;
        }
        if (binop->op == "and" || binop->op == "or") {
            return VarType::BOOL;
        }
        return left_type;
    }
    if (auto* unary = dynamic_cast<UnaryOp*>(&node)) {
        if (unary->op == "not") return VarType::BOOL;
        throw std::runtime_error("未知一元运算符类型 code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
        return VarType::NONE;
    }
    if (auto* call = dynamic_cast<Call*>(&node)) {
      return ctx.get_func_type(call->name).second;
    }
    throw std::runtime_error("未知表达式类型 code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
    return VarType::NONE;
}

void CodeGen::execute() {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    // 验证模块
    std::string error;
    llvm::raw_string_ostream error_stream(error);
    if (llvm::verifyModule(*module, &error_stream)) {
      ctx.add_error_front(ErrorHandler::ErrorLevel::Other,
                          "模块验证失败: " + error, -1, __FILE__, __LINE__);
      return;
    }
    
    auto jit = llvm::orc::LLJITBuilder().create();
    if (!jit) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法创建 JIT", 0, __FILE__, __LINE__);
        return;
    }

    auto& main_jd = (*jit)->getMainJITDylib();
    auto dlsg = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
        (*jit)->getDataLayout().getGlobalPrefix());
    if (!dlsg) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法创建动态库搜索生成器", 0, __FILE__, __LINE__);
        return;
    }
    main_jd.addGenerator(std::move(*dlsg));

    // Explicitly provide printf symbol
    llvm::orc::SymbolMap symbols;
    void* printf_ptr = dlsym(RTLD_DEFAULT, "printf");
    if (printf_ptr) {
        symbols[(*jit)->mangleAndIntern("printf")] = llvm::JITEvaluatedSymbol(
            llvm::pointerToJITTargetAddress(printf_ptr), llvm::JITSymbolFlags::Exported);
        if (auto err = main_jd.define(llvm::orc::absoluteSymbols(symbols))) {
            std::string err_msg;
            llvm::raw_string_ostream err_stream(err_msg);
            err_stream << err;
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法定义 printf 符号: " + err_msg, 0, __FILE__, __LINE__);
            return;
        }
    } else {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法找到 printf 符号，将依赖默认符号解析", 0, __FILE__, __LINE__);
        // Proceed without explicit mapping, relying on JIT's default resolution
    }

    if (auto err = (*jit)->addIRModule(
            llvm::orc::ThreadSafeModule(std::move(module), std::make_unique<llvm::LLVMContext>()))) {
        std::string err_msg;
        llvm::raw_string_ostream err_stream(err_msg);
        err_stream << err;
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法添加模块: " + err_msg, 0, __FILE__, __LINE__);
        return;
    }

    auto main = (*jit)->lookup("main");
    if (!main) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法找到 main 函数", 0, __FILE__, __LINE__);
        return;
    }

    using MainFn = int(*)();
    auto main_fn = reinterpret_cast<MainFn>(main->getAddress());
    main_fn();
}

void CodeGen::save_to_file(std::string& filename) {
    std::error_code EC;
    llvm::raw_fd_ostream OS(filename, EC);
    if (EC) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法打开文件: " + filename, 0, __FILE__, __LINE__);
        return;
    }
    module->print(OS, nullptr);
}

void CodeGen::compile_to_executable(std::string& output_filename, bool generate_object, bool generate_assembly) {
    // 初始化目标
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    // 验证模块
    std::string error;
    llvm::raw_string_ostream error_stream(error);
    if (llvm::verifyModule(*module, &error_stream)) {
        ctx.add_error_front(ErrorHandler::ErrorLevel::Other, "模块验证失败: " + error, -1, __FILE__, __LINE__);
        return;
    }

    // 设置目标机器
    auto target_triple = llvm::sys::getDefaultTargetTriple();
    module->setTargetTriple(target_triple);

    std::string target_error;
    auto target = llvm::TargetRegistry::lookupTarget(target_triple, target_error);
    if (!target) {
        ctx.add_error_front(ErrorHandler::ErrorLevel::Other, "无法查找目标: " + target_error, -1, __FILE__, __LINE__);
        return;
    }

    auto CPU = "generic";
    auto Features = "";
    llvm::TargetOptions opt;
    auto RM = llvm::Optional<llvm::Reloc::Model>();
    auto target_machine = target->createTargetMachine(target_triple, CPU, Features, opt, RM);

    module->setDataLayout(target_machine->createDataLayout());

    // 如果不是生成汇编或目标文件，我们需要一个临时目标文件
    std::string obj_file = generate_object ? output_filename : 
                          (output_filename + ".o");

    // 创建输出文件
    std::error_code EC;
    llvm::raw_fd_ostream dest(generate_assembly ? output_filename : obj_file, EC);
    if (EC) {
        ctx.add_error_front(ErrorHandler::ErrorLevel::Other, 
              "无法打开输出文件: " + (generate_assembly ? output_filename : obj_file), -1, __FILE__, __LINE__);
        return;
    }

    // 创建PassManager
    llvm::legacy::PassManager pass;
    pass.add(llvm::createVerifierPass());  // 添加验证Pass

    // 根据参数决定生成类型
    llvm::CodeGenFileType file_type = generate_assembly ? llvm::CGFT_AssemblyFile : llvm::CGFT_ObjectFile;
    
    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
        ctx.add_error_front(ErrorHandler::ErrorLevel::Other,
                        std::string("目标机器无法生成") +
                            (generate_assembly ? "汇编文件" : "目标文件"),
                        -1, __FILE__, __LINE__);
        return;
    }

    // 运行Pass
    pass.run(*module);
    dest.flush();

    // 如果不是生成汇编或目标文件，需要调用链接器生成可执行文件
    if (!generate_assembly && !generate_object) {
        // 构建链接命令
        std::string link_cmd = "ld -o " + output_filename + " " + obj_file;
        
        // 执行链接命令
        int result = std::system(link_cmd.c_str());
        if (result != 0) {
            ctx.add_error_front(ErrorHandler::ErrorLevel::Other, "链接失败", -1, __FILE__, __LINE__);
            return;
        }

        // 删除临时目标文件
        std::remove(obj_file.c_str());

        // 在Linux平台添加可执行权限
        if (PLATFORM_LINUX) {
            chmod(output_filename.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        }
    }
}

CodeGen::~CodeGen() {
    if (generate_debug_info && dbg_builder) {
        dbg_builder->finalize();
    }
}