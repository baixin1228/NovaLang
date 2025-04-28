#include "Print.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int Print::visit_stmt() {
    // 检查每个表达式的类型
    for (const auto& value : values) {
        std::shared_ptr<ASTNode> expr_ast;
        if (value->visit_expr(expr_ast) == -1) {
            return -1;
        }
    }
    return 0;
}

int Print::visit_expr(std::shared_ptr<ASTNode> &self) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "print语句不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
}

int Print::gencode_stmt() {
  // 创建格式化字符串和参数列表
  std::string format_str;
  std::vector<llvm::Value *> printf_args;

  // 遍历所有要打印的值
  for (const auto &expr : values) {
    std::shared_ptr<ASTNode> value_ast;
    llvm::Value* value = nullptr;
    if (expr->gencode_expr(VarType::NONE, value) == -1) {
      // 处理错误
      return -1;
    }
    
    expr->visit_expr(value_ast);

    // 根据类型添加对应的格式化字符串
    switch (value_ast->type) {
    case VarType::INT:
      format_str += "%ld ";
      printf_args.push_back(value);
      break;
    case VarType::FLOAT:
      format_str += "%g ";
      printf_args.push_back(value);
      break;
    case VarType::STRING: {
      // 对于字符串，调用运行时库的转换函数
      auto to_system_func =
          ctx.runtime_manager->getRuntimeFunction("string_to_system");
      if (!to_system_func) {
        auto memory_block_ptr_type = llvm::PointerType::get(
            ctx.runtime_manager->getNovaMemoryBlockType(), 0);
        to_system_func = llvm::Function::Create(
            llvm::FunctionType::get(
                ctx.builder->getInt8PtrTy(),  // 返回 char*
                {memory_block_ptr_type}, // 参数是 const nova_memory_block*
                false),
            llvm::Function::ExternalLinkage, "string_to_system", ctx.module.get());
      }

      // 确保value是unicode_string*类型，而不是i8*
      // 不需要类型转换，因为handleStringLiteral已经返回正确类型
      format_str += "%s ";
      printf_args.push_back(ctx.builder->CreateCall(to_system_func, {value}));
      break;
    }
    case VarType::BOOL: {
      // 使用条件选择而不是直接打印数字
      auto true_str = ctx.builder->CreateGlobalStringPtr("True", "true_str");
      auto false_str = ctx.builder->CreateGlobalStringPtr("False", "false_str");
      
      // 创建条件选择：如果bool值为true，选择True字符串，否则选择False字符串
      auto str_ptr = ctx.builder->CreateSelect(value, true_str, false_str);
      
      format_str += "%s ";
      printf_args.push_back(str_ptr);
      break;
    }
    default:
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "不支持的打印类型: " + var_type_to_string(value_ast->type), line,
                    __FILE__, __LINE__);
      return -1;
    }
  }

  // 添加换行符
  format_str += "\n";

  // 创建格式化字符串的全局变量
  auto fmt = ctx.builder->CreateGlobalStringPtr(format_str, "fmt_str");

  // 构建printf参数列表
  std::vector<llvm::Value *> args = {fmt};
  args.insert(args.end(), printf_args.begin(), printf_args.end());

  // 调用printf
  ctx.builder->CreateCall(ctx.printf_func, args);
  return 0;
}

int Print::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
    // Print statements don't produce a value
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "print语句不能作为表达式使用", line, __FILE__, __LINE__);
    ret_value = nullptr;
    return -1;
}