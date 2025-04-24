#include "For.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int For::visit_stmt(VarType &result) {
    VarType end_type;
    int ret = end->visit_expr(end_type);
    if (ret == -1) {
        return -1;
    }
    if (end_type != VarType::INT) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "for 循环范围必须是 int 类型，得到: " + var_type_to_string(end_type), line, __FILE__, __LINE__ );
        return -1;
    }
    add_var_info(iterator, true);
    auto& var_info = lookup_var_info(iterator);
    var_info.type = VarType::INT;
    for (auto& stmt : body) {
        if (stmt) {
            VarType stmt_result;
            int ret = stmt->visit_stmt(stmt_result);
            if (ret == -1) {
                return -1;
            }
        }
    }
    return 0;
}

int For::visit_expr(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "for 循环不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
}

int For::gencode_stmt() {
  auto iter_ptr =
      ctx.builder->CreateAlloca(ctx.builder->getInt64Ty(), nullptr, iterator);
  iter_ptr->setAlignment(llvm::Align(get_type_align(VarType::INT)));
  add_var_llvm_obj(iterator, iter_ptr);
  auto str = ctx.builder->CreateStore(ctx.builder->getInt64(0), iter_ptr);
  str->setAlignment(llvm::Align(get_type_align(VarType::INT)));

  auto loop_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "loop", ctx.current_function);
  auto body_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "body", ctx.current_function);
  auto exit_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "exit", ctx.current_function);

  ctx.builder->CreateBr(loop_bb);
  ctx.builder->SetInsertPoint(loop_bb);
  auto iter_val =
      ctx.builder->CreateLoad(ctx.builder->getInt64Ty(), iter_ptr, iterator + "_val");
  iter_val->setAlignment(llvm::Align(get_type_align(VarType::INT)));
  auto end_val = end->gencode_expr(VarType::NONE);
  auto cond = ctx.builder->CreateICmpSLT(iter_val, end_val, "cmp");
  ctx.builder->CreateCondBr(cond, body_bb, exit_bb);

  ctx.builder->SetInsertPoint(body_bb);
  for (auto &stmt : body) {
    if (stmt->gencode_stmt() == -1) {
      return -1;
    }
  }
  auto iter_new =
      ctx.builder->CreateAdd(iter_val, ctx.builder->getInt64(1), iterator + "_inc");
  auto str2 = ctx.builder->CreateStore(iter_new, iter_ptr);
  str2->setAlignment(llvm::Align(get_type_align(VarType::INT)));
  ctx.builder->CreateBr(loop_bb);

  ctx.builder->SetInsertPoint(exit_bb);
  return 0;
}

llvm::Value *For::gencode_expr(VarType expected_type) {
    return nullptr;
}