#include "Print.h"
#include "TypeChecker.h"
#include "ASTParser.h"
#include "ListLiteral.h"

// Element types (matching nova_element_type enum in Runtime/nova_list.h)
enum {
    NOVA_TYPE_NONE = 0,
    NOVA_TYPE_VOID = 1,
    NOVA_TYPE_INT = 2,
    NOVA_TYPE_FLOAT = 3,
    NOVA_TYPE_BOOL = 4,
    NOVA_TYPE_STRING = 5,
    NOVA_TYPE_STRUCT = 6,
    NOVA_TYPE_DICT = 7,
    NOVA_TYPE_LIST = 8,
    NOVA_TYPE_FUNCTION = 9,
    NOVA_TYPE_CLASS = 10,
    NOVA_TYPE_INSTANCE = 11
};

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

int Print::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "print语句不能作为表达式使用", line, __FILE__, __LINE__);
    return -1;
}

int Print::gencode_stmt() {
  // 创建格式化字符串和参数列表
  std::string format_str;
  std::vector<llvm::Value *> printf_args;

  // 遍历所有要打印的值
  for (const auto &expr : values) {
    std::shared_ptr<ASTNode> value_ast = nullptr;
    llvm::Value* value = nullptr;
    if (expr->gencode_expr(VarType::NONE, value) == -1) {
      // 处理错误
      return -1;
    }
    
    expr->visit_expr(value_ast);
    if (!value_ast) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "print 参数推导失败", line, __FILE__, __LINE__);
      return -1;
    }

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
    case VarType::DICT: {
      // 打印字典
      auto dict_str = ctx.builder->CreateGlobalStringPtr("{字典}", "dict_str");
      
      // 调用自定义的字典打印函数
      auto dict_to_str_func = ctx.module->getFunction("nova_dict_to_string");
      if (!dict_to_str_func) {
        auto memory_block_ptr_type = llvm::PointerType::get(
            ctx.runtime_manager->getNovaMemoryBlockType(), 0);
        dict_to_str_func = llvm::Function::Create(
            llvm::FunctionType::get(
                ctx.builder->getInt8PtrTy(),  // 返回 char*
                {memory_block_ptr_type},      // 参数是 nova_memory_block*
                false),
            llvm::Function::ExternalLinkage, "nova_dict_to_string", ctx.module.get());
      }
      
      // 如果dict_to_str_func已实现，则调用它，否则使用默认字符串
      if (dict_to_str_func) {
        auto str_ptr = ctx.builder->CreateCall(dict_to_str_func, {value});
        format_str += "%s ";
        printf_args.push_back(str_ptr);
      } else {
        format_str += "%s ";
        printf_args.push_back(dict_str);
      }
      break;
    }
    case VarType::LIST: {
      // 使用运行时函数打印数组
      auto list_str = ctx.builder->CreateGlobalStringPtr("[数组]", "list_str");
      
      // 获取列表的元素类型
      auto list_literal = std::dynamic_pointer_cast<ListLiteral>(value_ast);
      int elem_type_val = 0; // NOVA_TYPE_NONE
      
      if (list_literal) {
        // 转换VarType到nova_element_type枚举
        // 这里保持与nova_list.h中定义的nova_element_type枚举一致
        switch (list_literal->element_type) {
          case VarType::INT:
            elem_type_val = 2; // NOVA_TYPE_INT
            break;
          case VarType::FLOAT:
            elem_type_val = 3; // NOVA_TYPE_FLOAT
            break;
          case VarType::BOOL:
            elem_type_val = 4; // NOVA_TYPE_BOOL
            break;
          case VarType::STRING:
            elem_type_val = 5; // NOVA_TYPE_STRING
            break;
          case VarType::DICT:
            elem_type_val = 7; // NOVA_TYPE_DICT
            break;
          case VarType::LIST:
            elem_type_val = 8; // NOVA_TYPE_LIST
            break;
          case VarType::STRUCT:
            elem_type_val = 6; // NOVA_TYPE_STRUCT
            break;
          default:
            elem_type_val = 0; // NOVA_TYPE_NONE
            break;
        }
      }
      
      // 创建元素类型常量
      auto elem_type_const = ctx.builder->getInt32(elem_type_val);
      
      // 调用自定义的数组打印函数
      auto list_to_str_func = ctx.module->getFunction("nova_list_to_string");
      if (!list_to_str_func) {
        auto memory_block_ptr_type = llvm::PointerType::get(
            ctx.runtime_manager->getNovaMemoryBlockType(), 0);
        list_to_str_func = llvm::Function::Create(
            llvm::FunctionType::get(
                ctx.builder->getInt8PtrTy(),  // 返回 char*
                {
                  memory_block_ptr_type,      // 参数1: nova_memory_block*
                  ctx.builder->getInt32Ty()   // 参数2: 元素类型
                },
                false),
            llvm::Function::ExternalLinkage, "nova_list_to_string", ctx.module.get());
      }
      
      // 如果list_to_str_func已实现，则调用它，否则使用默认字符串
      if (list_to_str_func) {
        auto str_ptr = ctx.builder->CreateCall(list_to_str_func, {value, elem_type_const});
        format_str += "%s ";
        printf_args.push_back(str_ptr);
      } else {
        format_str += "%s ";
        printf_args.push_back(list_str);
      }
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