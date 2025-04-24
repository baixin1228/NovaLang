#include "StructLiteral.h"
#include "TypeChecker.h"
#include "Common.h"


int StructLiteral::visit_stmt(VarType &result) {
    result = VarType::STRUCT;
    return 0;
}

int StructLiteral::visit_expr(VarType &result) {
    // 遍历所有字段，确保每个字段的值都是有效的表达式
    for (auto& field : fields) {
        VarType field_type;
        if (field.second->visit_expr(field_type) == -1) {
            return -1;
        }
    }
    
    result = VarType::STRUCT;
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
    VarType field_type;
    field.second->visit_expr(field_type);
    // 计算字段值所需空间
    total_size += get_type_align(field_type);

    // 准备字段值
    auto field_value = field.second->gencode_expr(expected_type);
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