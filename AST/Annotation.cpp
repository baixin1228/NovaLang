#include "Annotation.h"
#include "Function.h"
#include "StructLiteral.h"
#include "../Error.h"
#include "../Context.h"
#include <memory>

int Annotation::visit_stmt() {
    // 注解节点不再直接调用目标的 visit_stmt
    // 由于注解已经被添加到目标节点（如函数）中，所以无需额外操作
    return 0;
}

int Annotation::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
    // 注解不能用于表达式
    ctx.add_error(ErrorHandler::ErrorLevel::SEMANTIC, 
                 "注解不能直接用于表达式", 
                 line, __FILE__, __LINE__);
    return -1;
}

int Annotation::gencode_stmt() {
    // 注解节点不生成实际代码
    return 0;
}

int Annotation::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
    // 注解不能用于表达式
    ctx.add_error(ErrorHandler::ErrorLevel::SEMANTIC, 
                 "注解不能直接生成表达式代码", 
                 line, __FILE__, __LINE__);
    return -1;
} 