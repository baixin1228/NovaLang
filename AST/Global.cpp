#include "Global.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int Global::visit_stmt() {
    // 将变量标记为全局变量
    for (const auto& var : vars) {
        add_global_var(var);
    }
    return 0;
}

int Global::visit_expr(std::shared_ptr<ASTNode> &self) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "global语句不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
}

int Global::gencode_stmt() { return 0; }

int Global::gencode_expr(VarType expected_type, llvm::Value *&value) {
    value = nullptr;
    return 0;
}