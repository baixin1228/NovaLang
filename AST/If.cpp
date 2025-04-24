#include "ASTParser.h"
#include "TypeChecker.h"
#include "If.h"

int If::visit_stmt(VarType &result) {
    VarType cond_type;
    int ret = condition->visit_expr(cond_type);
    if (ret == -1) {
        return -1;
    }
    if (cond_type != VarType::BOOL) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "if 条件必须是 bool 类型，得到: " + var_type_to_string(cond_type), line, __FILE__, __LINE__);
        return -1;
    }

    VarType return_type = VarType::NONE;
    for (auto& stmt : body) {
        VarType stmt_type;
        int ret = stmt->visit_stmt(stmt_type);
        if (ret == -1) {
            return -1;
        }
        if (stmt_type != VarType::NONE) {
            return_type = stmt_type;
        }
    }

    for (auto& elif : elifs) {
        VarType elif_cond_type;
        int ret = elif.first->visit_expr(elif_cond_type);
        if (ret == -1) {
            return -1;
        }
        if (elif_cond_type != VarType::BOOL) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                             "elif 条件必须是 bool 类型，得到: " + var_type_to_string(elif_cond_type), line, __FILE__, __LINE__);
            return -1;
        }
        for (auto& stmt : elif.second) {
            VarType stmt_type;
            int ret = stmt->visit_stmt(stmt_type);
            if (ret == -1) {
                return -1;
            }
            if (stmt_type != VarType::NONE) {
                return_type = stmt_type;
            }
        }
    }

    for (auto& stmt : else_body) {
        VarType stmt_type;
        int ret = stmt->visit_stmt(stmt_type);
        if (ret == -1) {
            return -1;
        }
        if (stmt_type != VarType::NONE) {
            return_type = stmt_type;
        }
    }

    result = return_type;
    return 0;
}

int If::visit_expr(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                           "if 语句不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
}

int If::gencode_stmt() {
  std::vector<llvm::BasicBlock *> elif_bbs;
  std::vector<llvm::BasicBlock *> body_bbs;
  llvm::BasicBlock *else_bb = nullptr;
  llvm::BasicBlock *end_bb =
  llvm::BasicBlock::Create(*ctx.llvm_context, "end", ctx.current_function);

  // If 分支
  auto then_bb = llvm::BasicBlock::Create(*ctx.llvm_context, "then", ctx.current_function);
  auto next_bb =
      elifs.empty() && else_body.empty() ? end_bb
      : elifs.empty()
          ? (else_bb =
                 llvm::BasicBlock::Create(*ctx.llvm_context, "else", ctx.current_function))
          : (elif_bbs.push_back(
                 llvm::BasicBlock::Create(*ctx.llvm_context, "elif1", ctx.current_function)),
             elif_bbs.back());
  auto cond = condition->gencode_expr(VarType::NONE);
  ctx.builder->CreateCondBr(cond, then_bb, next_bb);

  ctx.builder->SetInsertPoint(then_bb);
  for (auto &stmt : body) {
    if (stmt->gencode_stmt() == -1) {
      return -1;
    }
  }
  ctx.builder->CreateBr(end_bb);

  // Elif 分支
  for (size_t i = 0; i < elifs.size(); ++i) {
    ctx.builder->SetInsertPoint(elif_bbs[i]);
    auto elif_cond = condition->gencode_expr(VarType::NONE);
    auto elif_body_bb = llvm::BasicBlock::Create(
        *ctx.llvm_context, "elif_body" + std::to_string(i + 1), ctx.current_function);
    auto next_elif_bb =
        i + 1 < elifs.size()
            ? (elif_bbs.push_back(llvm::BasicBlock::Create(
                   *ctx.llvm_context, "elif" + std::to_string(i + 2), ctx.current_function)),
               elif_bbs.back())
        : else_body.empty()
            ? end_bb
            : (else_bb =
                   llvm::BasicBlock::Create(*ctx.llvm_context, "else", ctx.current_function));
    ctx.builder->CreateCondBr(elif_cond, elif_body_bb, next_elif_bb);

    ctx.builder->SetInsertPoint(elif_body_bb);
    for (auto &stmt : elifs[i].second) {
      if (stmt->gencode_stmt() == -1) {
        return -1;
      }
    }
    ctx.builder->CreateBr(end_bb);
  }

  // Else 分支
  if (!else_body.empty()) {
    ctx.builder->SetInsertPoint(else_bb);
    for (auto &stmt : else_body) {
      stmt->gencode_stmt();
    }
    ctx.builder->CreateBr(end_bb);
  }

  ctx.builder->SetInsertPoint(end_bb);
  return 0;
}

llvm::Value *If::gencode_expr(VarType expected_type) {
    return nullptr;
}