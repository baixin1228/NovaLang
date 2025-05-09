#include "StructFieldAccess.h"
#include "Common.h"
#include "Context.h"
#include "StructLiteral.h"
#include "Variable.h"

int StructFieldAccess::visit_stmt() {
  std::shared_ptr<ASTNode> ret_ast;
  return visit_expr(ret_ast);
}

int StructFieldAccess::visit_struct_expr(std::shared_ptr<ASTNode> &expr_ret,
                                         StructLiteral *struct_lit) {

  std::shared_ptr<ASTNode> struct_var = nullptr;

  auto field = std::find_if(
      struct_lit->fields.begin(), struct_lit->fields.end(),
      [&](const std::pair<std::string, std::shared_ptr<ASTNode>> &pair) {
        return pair.first == field_name;
      });
  if (field == struct_lit->fields.end()) {
    if (struct_lit->type == VarType::STRUCT) {
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "未定义的结构体字段: " + field_name, line, __FILE__,
                    __LINE__);
    }
    return -1;
  } else {
    if (field->second->visit_expr(struct_var) != 0) {
      return -1;
    }
    auto *field_struct = dynamic_cast<StructLiteral *>(struct_var.get());
    if (field_struct) {
      struct_signature = field_struct->name;
    }
    expr_ret = struct_var;
    type = struct_var->type;
  }
  return 0;
}

int StructFieldAccess::visit_class_expr(std::shared_ptr<ASTNode> &expr_ret,
                                        StructLiteral *struct_lit) {

  std::shared_ptr<ASTNode> attr_return = nullptr;

  auto attr_var = struct_lit->lookup_var(field_name, -1);
  if (attr_var) {
    auto *field_struct = dynamic_cast<StructLiteral *>(attr_var->node.get());
    if (field_struct) {
      struct_signature = field_struct->name;
    }
    expr_ret = attr_var->node;
    type = attr_var->node->type;
    return 0;
  }

  auto attribute = struct_lit->functions.find(field_name);
  if (attribute != struct_lit->functions.end()) {
    auto attr_ast = attribute->second;
    if (attr_ast->visit_expr(attr_return) != 0) {
      return -1;
    }
    auto *field_struct = dynamic_cast<StructLiteral *>(attr_return.get());
    if (field_struct) {
      struct_signature = field_struct->name;
    }
    expr_ret = attr_return;
    type = attr_return->type;
    return 0;
  }
  if (struct_lit->type == VarType::CLASS) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "未定义的类属性: " + field_name,
                  line, __FILE__, __LINE__);
  }
  return -1;
}

int StructFieldAccess::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
  // Check if struct_expr is a direct variable reference
  std::shared_ptr<ASTNode> struct_var = nullptr;
  if (struct_expr->visit_expr(struct_var) != 0) {
    return -1;
  }

  auto *struct_lit = dynamic_cast<StructLiteral *>(struct_var.get());
  if (struct_lit) {
    if (struct_lit->type == VarType::STRUCT) {
      struct_type = VarType::STRUCT;
      return visit_struct_expr(expr_ret, struct_lit);
    } else if (struct_lit->type == VarType::CLASS) {
      struct_type = VarType::CLASS;
      return visit_class_expr(expr_ret, struct_lit);
    } else if (struct_lit->type == VarType::INSTANCE) {
      int ret = visit_struct_expr(expr_ret, struct_lit);
      if (ret == 0) {
        struct_type = VarType::INSTANCE;
        return ret;
      }
      ret = visit_class_expr(expr_ret, struct_lit);
      if (ret == 0) {
        struct_type = VarType::CLASS;
        return ret;
      }
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "无法从实例访问字段: " + field_name, line, __FILE__,
                    __LINE__);
      return -1;
    }
  } else {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "无法从非结构体类型访问字段: " + field_name, line, __FILE__,
                  __LINE__);
  }

  return -1;
}

int StructFieldAccess::gencode_stmt() { return 0; }

int access_field_offset(std::shared_ptr<ASTNode> struct_node,
                        std::string field_name,
                        std::shared_ptr<ASTNode> &field_value,
                        size_t &field_offset, VarType &field_type) {
  field_offset = 0;
  bool found_field = false;

  // Try to determine field offset and type
  // This is more complex for nested access, as we need to check at runtime
  std::shared_ptr<ASTNode> struct_var = nullptr;
  struct_node->visit_expr(struct_var);
  if (struct_var) {
    auto *struct_lit = dynamic_cast<StructLiteral *>(struct_var.get());
    if (struct_lit) {
      std::cout << "struct field access: " << struct_lit->name << "."
                << field_name << std::endl;
      const auto &fields = struct_lit->fields;
      for (const auto &field : fields) {
        std::cout << "field: " << field.first << std::endl;
        if (field.first == field_name) {
          found_field = true;
          field_type = field.second->type;
          field.second->visit_expr(field_value);
          break;
        }
        field_offset += get_type_align(field.second->type);
      }
    } else {
      throw std::runtime_error("推断类型不是结构体: " + field_name + " file:" + std::string(__FILE__) + " line:" + std::to_string(__LINE__));
    }
  } else {
    throw std::runtime_error("推断结构体失败: " + field_name + " file:" + std::string(__FILE__) + " line:" + std::to_string(__LINE__));
  }

  // If we couldn't determine the field statically, we implement a more dynamic
  // approach For simplicity, we'll assume the field exists and try to access it
  if (!found_field) {
    // In a more complete implementation, we would search the struct at runtime
    // Here we'll provide a best-effort implementation
    print_backtrace();
    throw std::runtime_error("推断结构体字段类型失败: " + field_name + " file:" + std::string(__FILE__) + " line:" + std::to_string(__LINE__));
  }
  return 0;
}

int StructFieldAccess::gencode_class_expr(VarType expected_type,
                                          llvm::Value *&value) {
  std::shared_ptr<ASTNode> struct_var = nullptr;
  if (struct_expr->visit_expr(struct_var) != 0) {
    return -1;
  }
  auto var_info = struct_var->lookup_var(field_name, -1);
  if (!var_info) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "未定义的类属性: " + field_name, line, __FILE__, __LINE__);
    return -1;
  }
  return Variable::gencode_var_expr(ctx, field_name, line, expected_type, value, var_info);
}

int StructFieldAccess::gencode_struct_expr(VarType expected_type,
                                          llvm::Value *&value) {
  // 获取结构体引用
  llvm::Value *struct_val = nullptr;
  if (struct_expr->gencode_expr(VarType::STRUCT, struct_val) != 0) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                  "无法访问无效结构体的字段 " + field_name, line, __FILE__,
                  __LINE__);
    return -1;
  }

  // 获取结构体数据区域指针
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

  //   std::cout << "StructFieldAccess::gencode_expr: " << field_name << " "
  //             << var_type_to_string(field_type) << " offset: " <<
  //             field_offset
  //             << std::endl;

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
    field_val = ctx.builder->CreateLoad(ctx.builder->getInt64Ty(), field_ptr);
    break;
  }
  case VarType::FLOAT: {
    auto field_ptr = ctx.builder->CreateBitCast(
        field_ptr_ptr, llvm::PointerType::get(ctx.builder->getDoubleTy(), 0));
    field_val = ctx.builder->CreateLoad(ctx.builder->getDoubleTy(), field_ptr);
    break;
  }
  case VarType::BOOL: {
    auto field_ptr = ctx.builder->CreateBitCast(
        field_ptr_ptr, llvm::PointerType::get(ctx.builder->getInt1Ty(), 0));
    field_val = ctx.builder->CreateLoad(ctx.builder->getInt1Ty(), field_ptr);
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
    field_val = ctx.builder->CreateLoad(
        llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(),
                               0),
        field_ptr);
    break;
  }
  default:
    // For unknown types, try to load as a struct pointer
    auto field_ptr = ctx.builder->CreateBitCast(
        field_ptr_ptr,
        llvm::PointerType::get(
            llvm::PointerType::get(
                ctx.runtime_manager->getNovaMemoryBlockType(), 0),
            0));
    field_val = ctx.builder->CreateLoad(
        llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(),
                               0),
        field_ptr);
    break;
  }

  value = field_val;
  return 0;
}

int StructFieldAccess::gencode_expr(VarType expected_type,
                                    llvm::Value *&value) {
  if (struct_type == VarType::CLASS) {
    return gencode_class_expr(expected_type, value);
  } else {
    return gencode_struct_expr(expected_type, value);
  }
}