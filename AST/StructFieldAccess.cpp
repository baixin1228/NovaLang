#include "StructFieldAccess.h"
#include "StructLiteral.h"
#include "Variable.h"
#include "Context.h"
#include "Common.h"

int StructFieldAccess::visit_stmt() {
    std::shared_ptr<ASTNode> ret_ast;
    return visit_expr(ret_ast);
}

int StructFieldAccess::visit_expr(std::shared_ptr<ASTNode> &self) {
    // Check if struct_expr is a direct variable reference
    std::shared_ptr<ASTNode> struct_var = nullptr;
    if (struct_expr->visit_expr(struct_var) != 0) {
      return -1;
    }

    auto *struct_lit = dynamic_cast<StructLiteral *>(struct_var.get());
    if (struct_lit) {
        auto field = struct_lit->fields.find(field_name);
        if (field == struct_lit->fields.end()) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                        "未定义的结构体字段: " + field_name, line, __FILE__,
                        __LINE__);
        } else {
            if (field->second->visit_expr(struct_var) != 0) {
                return -1;
            }
            auto *field_struct = dynamic_cast<StructLiteral *>(struct_var.get());
            if (field_struct) {
                struct_signature = field_struct->name;
            }
            self = struct_var;
            type = struct_var->type;
            return 0;
        }
    } else {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                        "无法从非结构体类型访问字段: " + field_name, line,
                        __FILE__, __LINE__);
    }

    return -1;
}

int StructFieldAccess::gencode_stmt() { return 0; }

int access_field_offset(
    std::shared_ptr<ASTNode> struct_node, std::string field_name,
    std::shared_ptr<ASTNode> &field_value, size_t &field_offset,
    VarType &field_type) {
  field_offset = 0;
  bool found_field = false;

  // Try to determine field offset and type
  // This is more complex for nested access, as we need to check at runtime
  std::shared_ptr<ASTNode> struct_var = nullptr;
  struct_node->visit_expr(struct_var);
  if (struct_var) {
    auto *struct_lit = dynamic_cast<StructLiteral *>(struct_var.get());
    if (struct_lit) {
      std::cout << "struct field access: " << struct_lit->name << "." << field_name
                << std::endl;
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
      throw std::runtime_error("推断类型不是结构体: " + field_name);
    }
  } else {
    throw std::runtime_error("推断结构体失败: " + field_name);
  }

  // If we couldn't determine the field statically, we implement a more dynamic
  // approach For simplicity, we'll assume the field exists and try to access it
  if (!found_field) {
    // In a more complete implementation, we would search the struct at runtime
    // Here we'll provide a best-effort implementation
    throw std::runtime_error("推断字段类型失败: " + field_name);
  }
  return 0;
}

int StructFieldAccess::gencode_expr(VarType expected_type, llvm::Value *&value) {
    // 获取结构体引用
    llvm::Value *struct_val = nullptr;
    if (struct_expr->gencode_expr(VarType::STRUCT, struct_val) != 0) {
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                     "无法访问无效结构体的字段 " + field_name,
                     line, __FILE__, __LINE__);
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
    access_field_offset(struct_expr, field_name, field_value, field_offset, field_type);

    std::cout << "StructFieldAccess::gencode_expr: " << field_name << " " << var_type_to_string(field_type) 
    << " offset: " << field_offset << std::endl;

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
                field_ptr_ptr, llvm::PointerType::get(
                                    llvm::PointerType::get(
                                        ctx.runtime_manager->getNovaMemoryBlockType(), 0),
                                    0));
            field_val = ctx.builder->CreateLoad(
                llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0),
                field_ptr);
            break;
        }
        default:
            // For unknown types, try to load as a struct pointer
            auto field_ptr = ctx.builder->CreateBitCast(
                field_ptr_ptr, llvm::PointerType::get(
                                    llvm::PointerType::get(
                                        ctx.runtime_manager->getNovaMemoryBlockType(), 0),
                                    0));
            field_val = ctx.builder->CreateLoad(
                llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0),
                field_ptr);
            break;
    }

    value = field_val;
    return 0;
}