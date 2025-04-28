#include "StructFieldAssign.h"
#include "Common.h"
#include "Context.h"
#include "StructFieldAccess.h"
#include "StructLiteral.h"
#include "Variable.h"

int StructFieldAssign::visit_stmt() {
  // Validate structure expression
  std::shared_ptr<ASTNode> prev_ast;
  if (struct_expr->visit_expr(prev_ast) != 0) {
    return -1;
  }

  auto struct_ast = dynamic_cast<StructLiteral *>(prev_ast.get());
  if (!struct_ast) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "无法给非结构体类型的字段赋值", line, __FILE__, __LINE__);
    return -1;
  }

  // Validate value expression
  std::shared_ptr<ASTNode> value_ast;
  if (visit_expr(value_ast) != 0) {
    return -1;
  }

  auto field = struct_ast->fields.find(field_name);
  if (field == struct_ast->fields.end()) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "未定义的结构体字段: " + field_name, line, __FILE__,
                  __LINE__);
    return -1;
  }
  if (field->second->type != value_ast->type) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "赋值类型不匹配: " + field_name + " " +
                      var_type_to_string(field->second->type) + " " +
                      var_type_to_string(value_ast->type),
                  line, __FILE__, __LINE__);
    return -1;
  }

  type = value_ast->type;
  return 0;
}

int StructFieldAssign::visit_expr(std::shared_ptr<ASTNode> &self) {
  // For struct field assignment, result is the value expression's type
  value->visit_expr(self);
  return 0;
}

int StructFieldAssign::gencode_stmt() {
  // Get struct pointer - this could be a nested structure
  llvm::Value *struct_val = nullptr;
  if (struct_expr->gencode_expr(VarType::STRUCT, struct_val) != 0) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "无法获取结构体引用", line,
                  __FILE__, __LINE__);
    return -1;
  }

  // Get the value
  std::shared_ptr<ASTNode> value_ast;
  value->visit_expr(value_ast);
  llvm::Value *value_val = nullptr;
  if (value->gencode_expr(value_ast->type, value_val) != 0) {
    return -1;
  }

  // Get pointer to struct data area
  auto data_ptr = ctx.builder->CreateCall(
      ctx.runtime_manager->getRuntimeFunction("nova_memory_get_data"),
      {struct_val});
  auto byte_ptr = ctx.builder->CreateBitCast(
      data_ptr, llvm::PointerType::get(ctx.builder->getInt8Ty(), 0));

  size_t field_offset = 0;
  VarType field_type = VarType::NONE;
  std::shared_ptr<ASTNode> field_value;
  access_field_offset(struct_expr, field_name, field_value, field_offset,
                      field_type);

  // Store field value based on type and offset
  llvm::Value *field_ptr_ptr = ctx.builder->CreateGEP(
      ctx.builder->getInt8Ty(), byte_ptr,
      llvm::ConstantInt::get(ctx.builder->getInt64Ty(), field_offset));

  // Store value based on value type
  switch (field_type) {
  case VarType::INT: {
    auto field_ptr = ctx.builder->CreateBitCast(
        field_ptr_ptr, llvm::PointerType::get(ctx.builder->getInt64Ty(), 0));
    ctx.builder->CreateStore(value_val, field_ptr);
    break;
  }
  case VarType::FLOAT: {
    auto field_ptr = ctx.builder->CreateBitCast(
        field_ptr_ptr, llvm::PointerType::get(ctx.builder->getDoubleTy(), 0));
    ctx.builder->CreateStore(value_val, field_ptr);
    break;
  }
  case VarType::BOOL: {
    auto field_ptr = ctx.builder->CreateBitCast(
        field_ptr_ptr, llvm::PointerType::get(ctx.builder->getInt1Ty(), 0));
    ctx.builder->CreateStore(value_val, field_ptr);
    break;
  }
  case VarType::STRING:
  case VarType::STRUCT:
  case VarType::DICT:
  case VarType::LIST: {
    auto field_ptr = ctx.builder->CreateBitCast(
        field_ptr_ptr,
        llvm::PointerType::get(
            llvm::PointerType::get(
                ctx.runtime_manager->getNovaMemoryBlockType(), 0),
            0));
    ctx.builder->CreateStore(value_val, field_ptr);
    break;
  }
  default:
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "不支持的结构体字段类型: " +
                      var_type_to_string(value_ast->type),
                  line, __FILE__, __LINE__);
    return -1;
  }
  std::cout << "StructFieldAssign::gencode_stmt: " << field_name << " "
            << var_type_to_string(type) << std::endl;

  return 0;
}

int StructFieldAssign::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
  // Return the value for expression context
  return value->gencode_expr(expected_type, ret_value);
}