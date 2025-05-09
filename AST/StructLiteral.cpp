#include "StructLiteral.h"
#include "BoolLiteral.h"
#include "Common.h"
#include "FloatLiteral.h"
#include "IntLiteral.h"
#include "StringLiteral.h"
#include "TypeChecker.h"
#include <sstream>

// 为结构体字段生成类型签名
std::string StructLiteral::generateTypeSignature(
    const std::shared_ptr<ASTNode> &node) const {
  if (!node)
    return "null";

  // 根据节点的类型生成签名
  switch (node->type) {
  case VarType::INT:
    return "i64";
  case VarType::FLOAT:
    return "f64";
  case VarType::BOOL:
    return "i1";
  case VarType::STRING:
    return "string";
  case VarType::STRUCT:
    // 对于结构体类型，使用其名称作为签名
    if (auto structLit = dynamic_cast<StructLiteral *>(node.get())) {
      if (structLit->struct_type == StructType::CLASS) {
        return structLit->name;
      } else {
        if (structLit->name.length() > 7) {
          return structLit->name.substr(7);
        } else {
          return structLit->name;
        }
      }
    }
    return "struct";
  case VarType::DICT:
    return "dict";
  case VarType::LIST:
    return "list";
  case VarType::FUNCTION:
    // 对于函数类型，我们使用特定前缀来标识
    if (auto funcNode = dynamic_cast<Function *>(node.get())) {
      // 如果可以获取函数名称，使用名称作为函数签名的一部分
      return "func_" + funcNode->name;
    }
    return "func";
  default:
    return "unknown";
  }
}

int StructLiteral::visit_stmt() {
  std::shared_ptr<ASTNode> expr_ret;
  visit_expr(expr_ret);
  return 0;
}

int StructLiteral::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
  // 遍历所有字段，确保每个字段的值都是有效的表达式，并执行类型推导
  if (visit_count == 0) {
    std::stringstream signature;
    signature << "struct";

    for (auto &field : fields) {
      if (field.second) {
        std::shared_ptr<ASTNode> field_result;
        if (field.second->visit_expr(field_result) == -1) {
          return -1;
        }
        if (struct_type == StructType::STRUCT && name.empty()) {
          signature << "_" << field.first << "_"
                    << generateTypeSignature(field.second);
        }
      }
    }
    
    for (auto attr : attributes) {
      if (attr) {
        if (attr->visit_stmt() == -1) {
          return -1;
        }
      }
    }
    if (struct_type == StructType::STRUCT) {
      name = signature.str();
      type = VarType::STRUCT;
    } else {
      type = VarType::CLASS;
      for (auto func : functions) {
        if (func.second->visit_stmt() == -1) {
          return -1;
        }
      }
    }
    std::cout << "==== Visit struct: " << name
              << " visit_count: " << visit_count << " ====" << std::endl;
    auto struct_info = std::make_shared<ClassInfo>();
    struct_info->node = shared_from_this();
    add_struct(name, struct_info);
  }
  visit_count++;
  expr_ret = shared_from_this();
  return 0;
}

// int StructLiteral::visit_func(std::string func_name) {
//   auto func = functions.find(func_name);
//   if (func == functions.end()) {
//     return -1;
//   }
//   return func->second->visit_stmt();
// }

int StructLiteral::gencode_stmt() {
  if (struct_type == StructType::CLASS) {
    for (auto attr : attributes) {
      auto attr_node = std::dynamic_pointer_cast<Assign>(attr);
      if (attr_node) {
        if (attr_node->gencode_var() == -1) {
          return -1;
        }
      }
    }
    for (auto func : functions) {
      auto func_node = std::dynamic_pointer_cast<Function>(func.second);
      if (func_node->reference_count > 0) {
        if (func_node->gencode_stmt() == -1) {
          return -1;
        }
      }
    }
  }
  return 0;
}

int StructLiteral::gencode_expr(VarType expected_type, llvm::Value *&value) {
  if (llvm_instance)
  {
    value = llvm_instance;
    return 0;
  }
  // 计算所需内存大小 - 仅需存储字段值
  size_t total_size = 0;

  // 创建字段映射的数据结构，用于后续写入内存
  std::vector<std::pair<std::string, std::pair<VarType, llvm::Value *>>>
      field_data;

  // 遍历字段计算总大小
  for (const auto &field : fields) {
    std::shared_ptr<ASTNode> field_ast;
    int ret = field.second->visit_expr(field_ast);
    if (ret == -1) {
      return -1;
    }
    VarType field_type = field_ast->type;
    if (field.second->type == VarType::FUNCTION) {
      field_type = VarType::FUNCTION;
    }

    std::cout << "field_ast: " << field.first
              << " type: " << var_type_to_string(field_type) << std::endl;
    // 计算字段值所需空间
    total_size += get_type_align(field_type);

    // 准备字段值
    llvm::Value *field_value = nullptr;
    if (field.second->gencode_expr(field_type, field_value) != 0) {
      return -1;
    }
    field_data.push_back({field.first, {field_type, field_value}});
  }

  // 使用nova_memory_alloc创建一个结构体对象，分配所需内存
  auto nova_memory_alloc_func =
      ctx.runtime_manager->getRuntimeFunction("nova_memory_alloc");
  // 确保分配至少有1字节空间
  if (total_size == 0)
    total_size = 1;
  llvm::Value *size_val =
      llvm::ConstantInt::get(ctx.builder->getInt64Ty(), total_size);
  llvm::Value *struct_ptr =
      ctx.builder->CreateCall(nova_memory_alloc_func, {size_val}, "struct_obj");

  // 获取数据区域指针
  auto data_ptr = ctx.builder->CreateCall(
      ctx.runtime_manager->getRuntimeFunction("nova_memory_get_data"),
      {struct_ptr});
  auto byte_ptr = ctx.builder->CreateBitCast(
      data_ptr, llvm::PointerType::get(ctx.builder->getInt8Ty(), 0));

  // 当前偏移量
  size_t offset = 0;

  // 为每个字段设置值
  for (const auto &field_info : field_data) {
    VarType field_type = field_info.second.first;
    llvm::Value *field_value = field_info.second.second;

    // 根据字段类型确定存储方式
    switch (field_type) {
    case VarType::INT: {
      auto value_ptr_ptr = ctx.builder->CreateGEP(
          ctx.builder->getInt8Ty(), byte_ptr,
          llvm::ConstantInt::get(ctx.builder->getInt64Ty(), offset));
      auto value_ptr = ctx.builder->CreateBitCast(
          value_ptr_ptr, llvm::PointerType::get(ctx.builder->getInt64Ty(), 0));

      ctx.builder->CreateStore(field_value, value_ptr);
      break;
    }
    case VarType::FLOAT: {
      auto value_ptr_ptr = ctx.builder->CreateGEP(
          ctx.builder->getInt8Ty(), byte_ptr,
          llvm::ConstantInt::get(ctx.builder->getInt64Ty(), offset));
      auto value_ptr = ctx.builder->CreateBitCast(
          value_ptr_ptr, llvm::PointerType::get(ctx.builder->getDoubleTy(), 0));

      ctx.builder->CreateStore(field_value, value_ptr);
      break;
    }
    case VarType::BOOL: {
      auto value_ptr_ptr = ctx.builder->CreateGEP(
          ctx.builder->getInt8Ty(), byte_ptr,
          llvm::ConstantInt::get(ctx.builder->getInt64Ty(), offset));
      auto value_ptr = ctx.builder->CreateBitCast(
          value_ptr_ptr, llvm::PointerType::get(ctx.builder->getInt1Ty(), 0));

      ctx.builder->CreateStore(field_value, value_ptr);
      break;
    }
    case VarType::FUNCTION: {
      // 函数字段的处理：存储函数指针
      // 函数指针本质上是一个指向代码的指针，我们将其存储为指针类型
      auto value_ptr_ptr = ctx.builder->CreateGEP(
          ctx.builder->getInt8Ty(), byte_ptr,
          llvm::ConstantInt::get(ctx.builder->getInt64Ty(), offset));

      // 将函数指针转换为通用指针类型
      auto value_ptr = ctx.builder->CreateBitCast(
          value_ptr_ptr,
          llvm::PointerType::get(
              llvm::PointerType::get(ctx.builder->getInt8Ty(), 0), 0));

      // 如果function_value是函数指针，需要进行适当的转换
      auto func_ptr = ctx.builder->CreateBitCast(
          field_value, llvm::PointerType::get(ctx.builder->getInt8Ty(), 0));

      // 存储函数指针
      ctx.builder->CreateStore(func_ptr, value_ptr);
      // std::cout << "gen instance function: " << field_info.first << " func_ptr: " << func_ptr << " value_ptr: " << value_ptr << std::endl;
      break;
    }
    case VarType::STRING:
    case VarType::STRUCT:
    case VarType::DICT:
    case VarType::LIST: {
      auto value_ptr_ptr = ctx.builder->CreateGEP(
          ctx.builder->getInt8Ty(), byte_ptr,
          llvm::ConstantInt::get(ctx.builder->getInt64Ty(), offset));
      auto value_ptr = ctx.builder->CreateBitCast(
          value_ptr_ptr,
          llvm::PointerType::get(
              llvm::PointerType::get(
                  ctx.runtime_manager->getNovaMemoryBlockType(), 0),
              0));

      ctx.builder->CreateStore(field_value, value_ptr);

      // 对于引用类型，需要增加引用计数
      auto retain_func =
          ctx.runtime_manager->getRuntimeFunction("nova_memory_retain");
      ctx.builder->CreateCall(retain_func, {field_value});
      break;
    }
    default:
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "不支持的结构体字段类型: " + var_type_to_string(field_type),
                    line, __FILE__, __LINE__);
      return -1;
    }

    // 更新偏移量
    offset += get_type_align(field_type);
  }

  value = struct_ptr;
  llvm_instance = value;
  return 0;
}