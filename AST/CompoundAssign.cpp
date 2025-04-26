#include "CompoundAssign.h"
#include "Common.h"
#include "Context.h"
#include <iostream>
#include "Variable.h"
void CompoundAssign::print(int indent) {
    std::cout << std::string(indent, ' ') << "CompoundAssign(" << var << " " << op << ")" << std::endl;
    value->print(indent + 2);
}

int CompoundAssign::visit_stmt() {
    std::shared_ptr<ASTNode> value_ast;
    int ret = value->visit_expr(value_ast);
    if (ret == -1) {
        return -1;
    }

    auto var_node = lookup_var(var, line);
    if (var_node->type == VarType::NONE) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义的变量: " + var, line, __FILE__, __LINE__);
        return -1;
    }

    if (var_node->type != value_ast->type &&
        !(var_node->type == VarType::FLOAT && value_ast->type == VarType::INT)) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "变量：" + var +
                        " 类型不匹配，得到的类型: " + var_type_to_string(value_ast->type) +
                        " 期望类型是：" + var_type_to_string(var_node->type),
                    line, __FILE__, __LINE__);
      return -1;
    }

    return 0;
}

int CompoundAssign::visit_expr(std::shared_ptr<ASTNode> &self) {
    std::shared_ptr<ASTNode> value_ast;
    int ret = value->visit_expr(value_ast);
    if (ret == -1) {
        return -1;
    }

    auto var_node = lookup_var(var, line);
    if (var_node->type == VarType::NONE) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义的变量: " + var, line, __FILE__, __LINE__);
        return -1;
    }

    if (var_node->type != value_ast->type &&
        !(var_node->type == VarType::FLOAT && value_ast->type == VarType::INT)) {
      ctx.add_error(
          ErrorHandler::ErrorLevel::TYPE,
          "类型不匹配: " + var + " 是 " + var_type_to_string(var_node->type) +
              " 类型，但赋值表达式是 " + var_type_to_string(value_ast->type) + " 类型",
          line, __FILE__, __LINE__);
      return -1;
    }

    self = shared_from_this();
    type = var_node->type;
    return 0;
}

int CompoundAssign::gencode_stmt() {
  auto var_node = lookup_var(var, line);
  VarType type = var_node->type;
  if (type == VarType::NONE) {
    throw std::runtime_error("未定义的变量: " + var +
                             " source:" + std::to_string(line) +
                             " file:" + std::string(__FILE__) +
                             " line:" + std::to_string(__LINE__));
  }

  // 字符串类型不支持复合赋值运算
  if (type == VarType::STRING) {
    throw std::runtime_error(
        "不支持对字符串类型使用复合赋值运算: " + var + " code:" +
        std::to_string(line) + " line:" + std::to_string(__LINE__));
  }

  auto ptr = var_node->llvm_obj;
  if (!ptr) {
    throw std::runtime_error("未定义的变量: " + var +
                             " source:" + std::to_string(line) +
                             " file:" + std::string(__FILE__) +
                             " line:" + std::to_string(__LINE__));
  }

  llvm::Type *llvm_type;
  switch (type) {
  case VarType::INT:
    llvm_type = ctx.builder->getInt64Ty();
    break;
  case VarType::FLOAT:
    llvm_type = ctx.builder->getDoubleTy();
    break;
  case VarType::BOOL:
    llvm_type = ctx.builder->getInt1Ty();
    break;
  default:
    throw std::runtime_error("未知变量类型: " + var +
                             " code:" + std::to_string(line) +
                             " line:" + std::to_string(__LINE__));
    return -1;
  }
  auto old_val = ctx.builder->CreateLoad(llvm_type, ptr, var + "_old");
  old_val->setAlignment(llvm::Align(get_type_align(type)));
  auto right_val = value->gencode_expr(type);

  llvm::Value *new_val;
  if (op == "+=") {
    new_val =
        type == VarType::INT
            ? ctx.builder->CreateAdd(old_val, right_val, var + "_add")
            : ctx.builder->CreateFAdd(old_val, right_val,
                                 var + "_fadd");
  } else if (op == "-=") {
    new_val =
        type == VarType::INT
            ? ctx.builder->CreateSub(old_val, right_val, var + "_sub")
            : ctx.builder->CreateFSub(old_val, right_val,
                                 var + "_fsub");
  } else if (op == "*=") {
    new_val =
        type == VarType::INT
            ? ctx.builder->CreateMul(old_val, right_val, var + "_mul")
            : ctx.builder->CreateFMul(old_val, right_val,
                                 var + "_fmul");
  } else if (op == "/=") {
    new_val =
        type == VarType::INT
            ? ctx.builder->CreateSDiv(old_val, right_val, var + "_div")
            : ctx.builder->CreateFDiv(old_val, right_val,
                                 var + "_fdiv");
  } else {
    throw std::runtime_error("未知的复合赋值运算符: " + op +
                             " code:" + std::to_string(line) +
                             " line:" + std::to_string(__LINE__));
  }

  auto str = ctx.builder->CreateStore(new_val, ptr);
  str->setAlignment(llvm::Align(get_type_align(type)));
  return 0;
}

llvm::Value *CompoundAssign::gencode_expr(VarType expected_type) {
    return nullptr;
}