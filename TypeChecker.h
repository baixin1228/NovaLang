#pragma once
#include "Context.h"
#include <map>
#include <string>
#include <vector>

// 类型检查
class TypeChecker {
    Context &ctx;

  public:
    TypeChecker(Context &ctx) : ctx(ctx) {}

    int check();

    void add_error(ErrorHandler::ErrorLevel level, const std::string& msg, int line) {
        ctx.add_error(level, msg, line, __FILE__, __LINE__);
    }

    friend class ASTNode;
    friend class Assign;
    friend class BinOp;
    friend class UnaryOp;
    friend class Call;
    friend class Function;
    friend class If;
    friend class For;
    friend class While;
    friend class Return;
    friend class Print;
    friend class Variable;
    friend class IntLiteral;
    friend class FloatLiteral;
    friend class BoolLiteral;
    friend class Increment;
};
