#include "Return.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int Return::visit_stmt(VarType &result) {
    return value->visit_expr(result);
}

int Return::visit_expr(VarType &result) {
    if (value) {
        return value->visit_expr(result);
    }
    return 0;
}

int Return::gencode_stmt() {
  // 获取返回值
  auto return_value = value->gencode_expr(VarType::NONE);

  // 如果返回值是字符串类型，增加引用计数
  VarType return_type;
  visit_expr(return_type);
  if (return_type == VarType::STRING) {
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
            auto &var_info = lookup_var_info(var_name);
            VarType var_type = var_info.type;

            // 如果是字符串类型，需要减少引用计数
            if (var_type == VarType::STRING) {
              // 获取变量的LLVM符号
              auto var_ptr = lookup_var_llvm_obj(var_name);
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

llvm::Value *Return::gencode_expr(VarType expected_type) {
    return nullptr;
}