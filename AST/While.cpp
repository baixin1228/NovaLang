#include "While.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int While::visit_stmt() {
    std::shared_ptr<ASTNode> cond_ast;
    int ret = condition->visit_expr(cond_ast);
    if (ret == -1) {
        return -1;
    }
    if (cond_ast->type != VarType::BOOL) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "while 条件必须是 bool 类型，得到: " + var_type_to_string(cond_ast->type), line, __FILE__, __LINE__);
        return -1;
    }
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

int While::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "while 循环不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
}

int While::gencode_stmt() {
  auto loop_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "loop", ctx.current_function);
  auto body_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "body", ctx.current_function);
  auto exit_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "exit", ctx.current_function);

  ctx.builder->CreateBr(loop_bb);
  ctx.update_insert_point(loop_bb);
  
  llvm::Value *cond = nullptr;
  if (condition->gencode_expr(VarType::BOOL, cond) != 0) {
    return -1;
  }
  
  ctx.builder->CreateCondBr(cond, body_bb, exit_bb);

  ctx.update_insert_point(body_bb);
  for (auto &stmt : body) {
    if (stmt->gencode_stmt() == -1) {
      return -1;
    }
  }
  ctx.builder->CreateBr(loop_bb);

  ctx.update_insert_point(exit_bb);
  return 0;
}

int While::gencode_expr(VarType expected_type, llvm::Value *&value) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "while 循环不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
}