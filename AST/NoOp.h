#pragma once
#include "ASTNode.h"
#include "../Context.h"
#include "../Error.h"
#include <iostream>

// NoOp节点用于表示Python的pass语句，不执行任何操作
class NoOp : public ASTNode {
public:
    NoOp(Context &ctx, int ln) : ASTNode(ctx, ln) {
        type = VarType::NONE;
    }

    void print(int level) override {
        std::cout << std::string(level * 2, ' ') << "NoOp (pass) [行 " << line << "]\n";
    }

    int visit_stmt() override {
        // pass语句不执行任何操作，直接返回成功
        return 0;
    }

    int visit_expr(std::shared_ptr<ASTNode> &expr_ret) override {
        // pass不能用于表达式
        ctx.add_error(ErrorHandler::ErrorLevel::SEMANTIC, 
                     "pass语句不能用作表达式", 
                     line, __FILE__, __LINE__);
        return -1;
    }

    int gencode_stmt() override {
        // pass语句不生成任何代码
        return 0;
    }

    int gencode_expr(VarType expected_type, llvm::Value *&ret_value) override {
        // pass不能用于表达式
        ctx.add_error(ErrorHandler::ErrorLevel::SEMANTIC, 
                     "pass语句不能用作表达式", 
                     line, __FILE__, __LINE__);
        return -1;
    }
}; 