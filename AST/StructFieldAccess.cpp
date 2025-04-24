#include "StructFieldAccess.h"
#include "StructLiteral.h"
#include "Variable.h"
#include "Context.h"
#include "Common.h"

int StructFieldAccess::visit_stmt(VarType &result) {
    return visit_expr(result);
}

int StructFieldAccess::visit_expr(VarType &result) {
    auto* var_lit = dynamic_cast<Variable*>(struct_expr.get());
    auto struct_var = ctx.lookup_global_struct(var_lit->name);
    if (!struct_var) {
        throw std::runtime_error("未定义的结构体: " + var_lit->name + " code:" + std::to_string(line) + " file:" + __FILE__ + " line:" + std::to_string(__LINE__));
        return -1;
    }
    auto* struct_lit = dynamic_cast<StructLiteral*>(struct_var.get());
    auto filed = struct_lit->fields.find(field_name);
    if (filed == struct_lit->fields.end()) {
        throw std::runtime_error("未定义的结构体字段: " + field_name + " code:" + std::to_string(line) + " file:" + __FILE__ + " line:" + std::to_string(__LINE__));
        return -1;
    }
    return filed->second->visit_expr(result);
}

int StructFieldAccess::gencode_stmt() { return 0; }

llvm::Value *StructFieldAccess::gencode_expr(VarType expected_type) {
  // 获取结构体引用
  auto struct_val = struct_expr->gencode_expr(expected_type);
  if (!struct_val) {
    throw std::runtime_error(
        "无法访问无效结构体的字段 code:" + std::to_string(line) +
        " line:" + std::to_string(__LINE__));
    return nullptr;
  }

  // 获取字段名和字段类型
  VarType field_type;
  visit_expr(field_type);

  // 获取结构体数据区域指针
  auto data_ptr = ctx.builder->CreateCall(
      ctx.runtime_manager->getRuntimeFunction("nova_memory_get_data"),
      {struct_val});
  auto byte_ptr = ctx.builder->CreateBitCast(
      data_ptr, llvm::PointerType::get(ctx.builder->getInt8Ty(), 0));

  // 计算字段偏移量
  size_t field_offset = 0;
  bool found_field = false;

  // 从原始的结构体字面量中获取字段信息
  auto *var_lit = dynamic_cast<Variable *>(struct_expr.get());
  auto ast_lit = ctx.lookup_global_struct(var_lit->name);
  if (ast_lit) {
    auto *struct_lit = dynamic_cast<StructLiteral *>(ast_lit.get());
    const auto &fields = struct_lit->fields;
    for (const auto &field : fields) {
      if (field.first == field_name) {
        found_field = true;
        break;
      }
      VarType type_tmp;
      field.second->visit_expr(type_tmp);
      field_offset += get_type_align(type_tmp);
    }
  }

  if (!found_field) {
    throw std::runtime_error("结构体不存在字段: " + field_name +
                             " code:" + std::to_string(line) +
                             " line:" + std::to_string(__LINE__));
    return nullptr;
  }

  // 根据字段类型和偏移量获取字段值
  llvm::Value *field_ptr_ptr = ctx.builder->CreateGEP(
      ctx.builder->getInt8Ty(), byte_ptr,
      llvm::ConstantInt::get(ctx.builder->getInt64Ty(), field_offset));

  llvm::Value *field_val = nullptr;

  // 根据字段类型加载值
  switch (field_type) {
  case VarType::INT: {
    auto field_ptr = ctx.builder->CreateBitCast(
        field_ptr_ptr, llvm::PointerType::get(ctx.builder->getInt64Ty(), 0));
    field_val = ctx.builder->CreateLoad(ctx.builder->getInt64Ty(), field_ptr,
                                   "field_" + field_name);
    break;
  }
  case VarType::FLOAT: {
    auto field_ptr = ctx.builder->CreateBitCast(
        field_ptr_ptr, llvm::PointerType::get(ctx.builder->getDoubleTy(), 0));
    field_val = ctx.builder->CreateLoad(ctx.builder->getDoubleTy(), field_ptr,
                                   "field_" + field_name);
    break;
  }
  case VarType::BOOL: {
    auto field_ptr = ctx.builder->CreateBitCast(
        field_ptr_ptr, llvm::PointerType::get(ctx.builder->getInt1Ty(), 0));
    field_val = ctx.builder->CreateLoad(ctx.builder->getInt1Ty(), field_ptr,
                                   "field_" + field_name);
    break;
  }
  case VarType::STRING:
  case VarType::STRUCT:
  case VarType::DICT:
  case VarType::LIST: {
    auto field_ptr = ctx.builder->CreateBitCast(
        field_ptr_ptr, llvm::PointerType::get(
                           llvm::PointerType::get(
                               ctx.runtime_manager->getNovaMemoryBlockType(), 0),
                           0));
    field_val = ctx.builder->CreateLoad(
        llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0),
        field_ptr, "field_" + field_name);
    break;
  }
  default:
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "不支持的结构体字段类型: " + var_type_to_string(field_type),
                  line, __FILE__, __LINE__);
    return nullptr;
  }

  return field_val;
}