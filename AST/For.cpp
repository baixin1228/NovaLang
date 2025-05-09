#include "For.h"
#include "TypeChecker.h"
#include "ASTParser.h"
#include "Variable.h"
int For::visit_stmt() {
    std::shared_ptr<ASTNode> end_ast;
    int ret = end->visit_expr(end_ast);
    if (ret == -1) {
        return -1;
    }
    if (end_ast->type != VarType::INT) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "for 循环范围必须是 int 类型，得到: " + var_type_to_string(end_ast->type), line, __FILE__, __LINE__ );
        return -1;
    }
    auto iterator_info = std::make_shared<VarInfo>();
    iterator_info->line = line;
    iterator_info->node = iterator;
    add_var(iterator->name, iterator_info, true);
    iterator->type = VarType::INT;
    std::shared_ptr<ASTNode> iterator_ast;
    ret = iterator->visit_expr(iterator_ast);
    if (ret == -1) {
      return -1;
    }
    auto var_info = lookup_var(iterator->name, line);;
    var_info->node->type = VarType::INT;
    for (auto& stmt : body) {
        if (stmt) {
            int ret = stmt->visit_stmt();
            if (ret == -1) {
                return -1;
            }
        }
    }
    return 0;
}

int For::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "for 循环不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
}

int For::gencode_stmt() {
  auto iter_ptr =
      ctx.builder->CreateAlloca(ctx.builder->getInt64Ty(), nullptr, iterator->name);
  iter_ptr->setAlignment(llvm::Align(get_type_align(VarType::INT)));
  auto var_info = lookup_var(iterator->name, line);
  var_info->llvm_obj = iter_ptr;
  auto str = ctx.builder->CreateStore(ctx.builder->getInt64(0), iter_ptr);
  str->setAlignment(llvm::Align(get_type_align(VarType::INT)));

  auto loop_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "loop", ctx.current_function);
  auto body_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "body", ctx.current_function);
  auto exit_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "exit", ctx.current_function);

  ctx.builder->CreateBr(loop_bb);
  ctx.builder->SetInsertPoint(loop_bb);
  auto iter_val =
      ctx.builder->CreateLoad(ctx.builder->getInt64Ty(), iter_ptr, iterator->name + "_val");
  iter_val->setAlignment(llvm::Align(get_type_align(VarType::INT)));
  llvm::Value* end_val = nullptr;
  int ret = end->gencode_expr(VarType::NONE, end_val);
  if (ret == -1) {
    return -1;
  }
  auto cond = ctx.builder->CreateICmpSLT(iter_val, end_val, "cmp");
  ctx.builder->CreateCondBr(cond, body_bb, exit_bb);

  ctx.builder->SetInsertPoint(body_bb);
  for (auto &stmt : body) {
    if (stmt->gencode_stmt() == -1) {
      return -1;
    }
  }
  auto iter_new =
      ctx.builder->CreateAdd(iter_val, ctx.builder->getInt64(1), iterator->name + "_inc");
  auto str2 = ctx.builder->CreateStore(iter_new, iter_ptr);
  str2->setAlignment(llvm::Align(get_type_align(VarType::INT)));
  ctx.builder->CreateBr(loop_bb);

  ctx.builder->SetInsertPoint(exit_bb);
  return 0;
}

int For::gencode_expr(VarType expected_type, llvm::Value *&value) {
  value = nullptr;
  return 0;
}