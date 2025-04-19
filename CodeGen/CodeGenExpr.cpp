#include "CodeGen.h"
#include "IntLiteral.h"
#include "FloatLiteral.h"
#include "BoolLiteral.h"
#include "Variable.h"
#include "BinOp.h"
#include "UnaryOp.h"
#include "Call.h"

llvm::Value* CodeGen::visit_expr(ASTNode& node, VarType expected_type) {
    if (auto* int_lit = dynamic_cast<IntLiteral*>(&node)) {
      if (expected_type != VarType::NONE && expected_type == VarType::FLOAT)
      {
        // 隐式类型转换
        return builder.CreateSIToFP(builder.getInt64(int_lit->value),
                                    builder.getDoubleTy());
      }
      return builder.getInt64(int_lit->value);
    }
    if (auto* float_lit = dynamic_cast<FloatLiteral*>(&node)) {
        return llvm::ConstantFP::get(builder.getDoubleTy(), float_lit->value);
    }
    if (auto* bool_lit = dynamic_cast<BoolLiteral*>(&node)) {
        return builder.getInt1(bool_lit->value);
    }
    if (auto* var = dynamic_cast<Variable*>(&node)) {
        auto ptr = var->lookup_llvm_symbol(var->name);
        if (!ptr) {
            throw std::runtime_error("未定义的变量: " + var->name + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
            return nullptr;
        }

        VarType type = var->lookup_var_type(var->name);
        switch (type) {
            case VarType::INT: {
                auto load = builder.CreateLoad(builder.getInt64Ty(), ptr, var->name + "_load");
                load->setAlignment(llvm::Align(get_type_align(type)));
                if (expected_type != VarType::NONE &&
                    expected_type == VarType::FLOAT) {
                    //隐式类型转换
                  return builder.CreateSIToFP(load, builder.getDoubleTy());
                }
                return load;
            }
            case VarType::FLOAT: {
                auto load = builder.CreateLoad(builder.getDoubleTy(), ptr, var->name + "_load");
                load->setAlignment(llvm::Align(get_type_align(type)));
                return load;
            }
            case VarType::BOOL: {
                auto load = builder.CreateLoad(builder.getInt1Ty(), ptr, var->name + "_load");
                load->setAlignment(llvm::Align(get_type_align(type)));
                return load;
            }
            default:
                throw std::runtime_error("未知变量类型: " + var->name + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
                return nullptr;
        }
    }
    if (auto* binop = dynamic_cast<BinOp*>(&node)) {
      VarType type = visit_expr_type(*binop->left);
      VarType left_expected_type = VarType::NONE;
      VarType right_expected_type = VarType::NONE;
      auto left_type = visit_expr_type(*binop->left);
      auto right_type = visit_expr_type(*binop->right);
      if (expected_type != VarType::NONE) {
        left_expected_type = expected_type;
        right_expected_type = expected_type;
        type = expected_type;
      }
      if (left_type != right_type) {
        if (left_type == VarType::FLOAT) {
          right_expected_type = VarType::FLOAT;
        }
        if (right_type == VarType::FLOAT) {
          left_expected_type = VarType::FLOAT;
        }
        type = VarType::FLOAT;
      }
      if (binop->op == "/") {
        left_expected_type = VarType::FLOAT;
        right_expected_type = VarType::FLOAT;
        type = VarType::FLOAT;
      }
      auto left = visit_expr(*binop->left, left_expected_type);
      auto right = visit_expr(*binop->right, right_expected_type);
        if (left == nullptr || right == nullptr) {
        throw std::runtime_error(
            "未知表达式 code:" + std::to_string(node.line) +
            " line:" + std::to_string(__LINE__));
        return nullptr;
        }
        if (binop->op == "+") {
            return type == VarType::INT ?
                builder.CreateAdd(left, right, "add") :
                builder.CreateFAdd(left, right, "fadd");
        }
        if (binop->op == "-") {
            return type == VarType::INT ?
                builder.CreateSub(left, right, "sub") :
                builder.CreateFSub(left, right, "fsub");
        }
        if (binop->op == "*") {
            return type == VarType::INT ?
                builder.CreateMul(left, right, "mul") :
                builder.CreateFMul(left, right, "fmul");
        }
        if (binop->op == "/") {
            return builder.CreateFDiv(left, right, "fdiv");
        }
        if (binop->op == "//") {
          return builder.CreateSDiv(left, right, "sdiv");
        }
        if (binop->op == "==") {
            if (type == VarType::INT) {
                return builder.CreateICmpEQ(left, right, "eq");
            } else if (type == VarType::FLOAT) {
                return builder.CreateFCmpOEQ(left, right, "feq");
            } else {
                return builder.CreateICmpEQ(left, right, "eq");
            }
        }
        if (binop->op == "<") {
            return type == VarType::INT ?
                builder.CreateICmpSLT(left, right, "cmp") :
                builder.CreateFCmpOLT(left, right, "fcmp");
        }
        if (binop->op == ">") {
            return type == VarType::INT ?
                builder.CreateICmpSGT(left, right, "cmp") :
                builder.CreateFCmpOGT(left, right, "fcmp");
        }
        if (binop->op == ">=") {
            return type == VarType::INT ?
                builder.CreateICmpSGE(left, right, "cmp") :
                builder.CreateFCmpOGE(left, right, "fcmp");
        }
        if (binop->op == "and") {
            return builder.CreateAnd(left, right, "and");
        }
        if (binop->op == "or") {
            return builder.CreateOr(left, right, "or");
        }
        throw std::runtime_error("未知运算符: " + binop->op + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
        return nullptr;
    }
    if (auto* unary = dynamic_cast<UnaryOp*>(&node)) {
        auto expr = visit_expr(*unary->expr);
        if (unary->op == "not") {
            return builder.CreateNot(expr, "not");
        }
        throw std::runtime_error("未知一元运算符: " + unary->op + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
        return nullptr;
    }
    if (auto* call = dynamic_cast<Call*>(&node)) {
        auto func = functions[call->name];
        std::vector<llvm::Value*> args;
        for (auto& arg : call->args) {
            args.push_back(visit_expr(*arg));
        }
        return builder.CreateCall(func, args, "call");
    }
    throw std::runtime_error("未知表达式 code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
    return nullptr;
}

VarType CodeGen::visit_expr_type(ASTNode& node) {
    if (auto* int_lit = dynamic_cast<IntLiteral*>(&node)) {
        return VarType::INT;
    }
    if (auto* float_lit = dynamic_cast<FloatLiteral*>(&node)) {
        return VarType::FLOAT;
    }
    if (auto* bool_lit = dynamic_cast<BoolLiteral*>(&node)) {
        return VarType::BOOL;
    }
    if (auto* var = dynamic_cast<Variable*>(&node)) {
      VarType type = var->lookup_var_type(var->name);
      if (type == VarType::NONE) {
        throw std::runtime_error("未定义的变量: " + var->name + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
        return VarType::NONE;
        }
        return type;
    }
    if (auto* binop = dynamic_cast<BinOp*>(&node)) {
      VarType left_type = visit_expr_type(*binop->left);
      if (binop->op == "==" || binop->op == "<" || binop->op == ">" ||
          binop->op == ">=") {
        return VarType::BOOL;
        }
        if (binop->op == "and" || binop->op == "or") {
            return VarType::BOOL;
        }
        return left_type;
    }
    if (auto* unary = dynamic_cast<UnaryOp*>(&node)) {
        if (unary->op == "not") return VarType::BOOL;
        throw std::runtime_error("未知一元运算符类型 code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
        return VarType::NONE;
    }
    if (auto* call = dynamic_cast<Call*>(&node)) {
      return ctx.get_func_type(call->name).second;
    }
    throw std::runtime_error("未知表达式类型 code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
    return VarType::NONE;
} 