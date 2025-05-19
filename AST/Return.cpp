#include "Return.h"
#include "TypeChecker.h"
#include "ASTParser.h"
#include "Variable.h"
#include "Void.h"

int Return::visit_stmt() {
    std::shared_ptr<ASTNode> value_ast;
    return visit_expr(value_ast);
}

int Return::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
  if (value) {
    int ret = value->visit_expr(expr_ret);
    type = expr_ret->type;
    return ret;
  } else {
    throw std::runtime_error("返回值为空: " + std::to_string(line) + " " + std::string(__FILE__) + " " + std::to_string(__LINE__));
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "返回值为空", line, __FILE__,
                  __LINE__);
    return -1;
  }
}

int Return::gencode_stmt() {
  if (type == VarType::NONE) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "返回值类型为空", line, __FILE__, __LINE__);
    return -1;
  }

  // 如果返回值为空，创建void返回
  if (type == VarType::VOID) {
    ctx.builder->CreateRetVoid();
    return 0;
  }

  // 获取返回值
  llvm::Value* return_value = nullptr;
  if (value->gencode_expr(VarType::NONE, return_value) == -1) {
    // 处理错误
    return -1;
  }

  // 如果返回值是字符串类型，增加引用计数
  std::shared_ptr<ASTNode> return_ast;
  visit_expr(return_ast);
  if (return_ast->type == VarType::STRING) {
    auto retain_func =
        ctx.runtime_manager->getRuntimeFunction("nova_memory_retain");
    if (retain_func) {
      ctx.builder->CreateCall(retain_func, {return_value});
    }
  }

  // 释放当前作用域中的字符串变量
  auto release_func =
      ctx.runtime_manager->getRuntimeFunction("nova_memory_release");
  if (release_func && ctx.current_function) {
    auto memory_block_ptr_type =
        llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0);

    // 遍历当前作用域中的所有变量
    for (llvm::BasicBlock &BB : *ctx.current_function) {
      for (llvm::Instruction &I : BB) {
        // 检查是否是分配指令
        if (auto *allocaInst = llvm::dyn_cast<llvm::AllocaInst>(&I)) {
          std::string var_name = allocaInst->getName().str();
          if (!var_name.empty()) {
            auto var_node = lookup_var(var_name, line);
            VarType var_type = var_node->node->type;

            // 如果是字符串类型，需要减少引用计数
            if (var_type == VarType::STRING) {
              // 获取变量的LLVM符号
              auto var_ptr = var_node->llvm_obj;
              if (var_ptr) {
                // 加载变量值
                auto var_value = ctx.builder->CreateLoad(
                    memory_block_ptr_type, var_ptr, var_name + "_load");

                // 创建判断：如果变量不为空，则减少引用计数
                auto is_not_null = ctx.builder->CreateIsNotNull(
                    var_value, var_name + "_is_not_null");
                auto release_block = llvm::BasicBlock::Create(
                    *ctx.llvm_context, var_name + "_release", ctx.current_function);
                auto continue_block = llvm::BasicBlock::Create(
                    *ctx.llvm_context, var_name + "_continue", ctx.current_function);

                ctx.builder->CreateCondBr(is_not_null, release_block,
                                     continue_block);

                // 在release块中减少引用计数
                ctx.builder->SetInsertPoint(release_block);
                ctx.builder->CreateCall(release_func, {var_value});
                ctx.builder->CreateBr(continue_block);

                // 继续处理下一个变量
                ctx.builder->SetInsertPoint(continue_block);
              }
            }
          }
        }
      }
    }
  }

  // 创建返回指令
  ctx.builder->CreateRet(return_value);
  return 0;
}

int Return::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
    // Return statements don't produce a value in expression context
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "return语句不能作为表达式使用", line, __FILE__, __LINE__);
    ret_value = nullptr;
    return -1;
}