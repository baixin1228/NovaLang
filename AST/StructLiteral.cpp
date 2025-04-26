#include "StructLiteral.h"
#include "TypeChecker.h"
#include "Common.h"
#include "IntLiteral.h"
#include "FloatLiteral.h"
#include "BoolLiteral.h"
#include "StringLiteral.h"
#include <sstream>

// 为结构体字段生成类型签名
std::string StructLiteral::generateTypeSignature(const std::shared_ptr<ASTNode>& node) const {
    if (!node) return "null";
    
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
            if (auto structLit = dynamic_cast<StructLiteral*>(node.get())) {
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
            if (auto funcNode = dynamic_cast<Function*>(node.get())) {
                // 如果可以获取函数名称，使用名称作为函数签名的一部分
                return "func_" + funcNode->name;
            }
            return "func";
        default:
            return "unknown";
    }
}

int StructLiteral::visit_stmt() {
    type = VarType::STRUCT;
    return 0;
}

int StructLiteral::visit_expr(std::shared_ptr<ASTNode> &self) {
    // 遍历所有字段，确保每个字段的值都是有效的表达式，并执行类型推导
    std::stringstream signature;
    signature << "struct";

    for (auto& field : fields) {
        if (field.second) {
            std::shared_ptr<ASTNode> field_ast;
            if (field.second->visit_expr(field_ast) == -1) {
                return -1;
            }
            if (struct_type == StructType::STRUCT && name.empty()) {
              signature << "_" << generateTypeSignature(field_ast);
            }
        }
    }
    name = signature.str();
    if (struct_type == StructType::STRUCT) {
      ctx.add_global_struct(name, shared_from_this());
    }
    type = VarType::STRUCT;
    self = shared_from_this();
    return 0;
}

int StructLiteral::gencode_stmt() { return 0; }

llvm::Value *StructLiteral::gencode_expr(VarType expected_type) {
  // 计算所需内存大小 - 仅需存储字段值
  size_t total_size = 0;

  // 创建字段映射的数据结构，用于后续写入内存
  std::vector<std::pair<std::string, std::pair<VarType, llvm::Value *>>>
      field_data;

  // 遍历字段计算总大小
  for (const auto &field : fields) {
    std::shared_ptr<ASTNode> field_ast;
    field.second->visit_expr(field_ast);
    std::cout << "field_ast: " << field.first << " type: " << var_type_to_string(field_ast->type) << std::endl;
    // 计算字段值所需空间
    total_size += get_type_align(field_ast->type);

    // 准备字段值
    auto field_value = field.second->gencode_expr(field_ast->type);
    field_data.push_back({field.first, {field_ast->type, field_value}});
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
          value_ptr_ptr, llvm::PointerType::get(
              llvm::PointerType::get(ctx.builder->getInt8Ty(), 0), 0));
      
      // 如果function_value是函数指针，需要进行适当的转换
      auto func_ptr = ctx.builder->CreateBitCast(
          field_value, 
          llvm::PointerType::get(ctx.builder->getInt8Ty(), 0));
      
      // 存储函数指针
      ctx.builder->CreateStore(func_ptr, value_ptr);
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
          value_ptr_ptr, llvm::PointerType::get(
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
      break;
    }

    // 更新偏移量
    offset += get_type_align(field_type);
  }

  return struct_ptr;
}