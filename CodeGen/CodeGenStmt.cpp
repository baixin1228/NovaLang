#include "CodeGen.h"
#include "Assign.h"
#include "BinOp.h"
#include "BoolLiteral.h"
#include "Call.h"
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
#include "AST/CompoundAssign.h"

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
    } else if (auto* comp_assign = dynamic_cast<CompoundAssign*>(&node)) {
        VarType type = comp_assign->lookup_var_type(comp_assign->var);
        if (type == VarType::NONE) {
            throw std::runtime_error("未定义的变量: " + comp_assign->var + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
        }

        auto ptr = comp_assign->lookup_llvm_symbol(comp_assign->var);
        if (!ptr) {
            throw std::runtime_error("未定义的变量: " + comp_assign->var + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
        }

        llvm::Type* llvm_type;
        switch (type) {
          case VarType::INT:
            llvm_type = builder.getInt64Ty();
            break;
          case VarType::FLOAT:
            llvm_type = builder.getDoubleTy();
            break;
          case VarType::BOOL:
            llvm_type = builder.getInt1Ty();
            break;
          default:
            throw std::runtime_error("未知变量类型: " + comp_assign->var + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
            return -1;
        }
        auto old_val = builder.CreateLoad(llvm_type, ptr, comp_assign->var + "_old");
        old_val->setAlignment(llvm::Align(get_type_align(type)));
        auto right_val = visit_expr(*comp_assign->value, type);
        
        llvm::Value* new_val;
        if (comp_assign->op == "+=") {
            new_val = type == VarType::INT ? 
                builder.CreateAdd(old_val, right_val, comp_assign->var + "_add") :
                builder.CreateFAdd(old_val, right_val, comp_assign->var + "_fadd");
        } else if (comp_assign->op == "-=") {
            new_val = type == VarType::INT ? 
                builder.CreateSub(old_val, right_val, comp_assign->var + "_sub") :
                builder.CreateFSub(old_val, right_val, comp_assign->var + "_fsub");
        } else if (comp_assign->op == "*=") {
            new_val = type == VarType::INT ? 
                builder.CreateMul(old_val, right_val, comp_assign->var + "_mul") :
                builder.CreateFMul(old_val, right_val, comp_assign->var + "_fmul");
        } else if (comp_assign->op == "/=") {
            new_val = type == VarType::INT ? 
                builder.CreateSDiv(old_val, right_val, comp_assign->var + "_div") :
                builder.CreateFDiv(old_val, right_val, comp_assign->var + "_fdiv");
        } else {
            throw std::runtime_error("未知的复合赋值运算符: " + comp_assign->op + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
        }

        auto str = builder.CreateStore(new_val, ptr);
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