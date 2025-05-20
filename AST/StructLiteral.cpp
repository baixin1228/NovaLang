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
  if (type != VarType::NONE) {
    expr_ret = shared_from_this();
    return 0;
  }
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
        if (func.second->is_abstract) {
          is_abstract = true;
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
  /* avoid circular dependency */
  if (is_class_resolved) {
    return 0;
  }
  is_class_resolved = true;

  if (struct_type == StructType::CLASS) {
    StructLiteral *parent_class = nullptr;
    if (!class_parent_name.empty()) {
      auto parent_class_info = lookup_struct(class_parent_name);
      if (parent_class_info) {
        parent_class = dynamic_cast<StructLiteral *>(parent_class_info->node.get());
        if (parent_class) {
          parent_class->gencode_stmt();
          /* copy parent class vtable */
          vtable.insert(vtable.end(), parent_class->vtable.begin(), parent_class->vtable.end());
        }
      }
    }
    /* generate attributes */
    for (auto attr : attributes) {
      auto attr_node = std::dynamic_pointer_cast<Assign>(attr);
      if (attr_node) {
        if (attr_node->gencode_var() == -1) {
          return -1;
        }
      }
    }
    /* generate functions */
    for (auto func : functions) {
      auto func_node = std::dynamic_pointer_cast<Function>(func.second);
      if (func_node->reference_count > 0) {
        if (func_node->gencode_stmt() == -1) {
          return -1;
        }
        auto it = std::find_if(vtable.begin(), vtable.end(), [func](const std::pair<std::string, std::shared_ptr<Function>> &item) {
          return item.first == func.first;
        });
        if (it == vtable.end()) {
          vtable.push_back(func);
        } else {
          it->second = func.second;
        }
      }
    }

    /* generate vtable */
    if (!vtable.empty()) {
      size_t vtable_size = vtable.size() * sizeof(void*);
      
      auto alloc_func = ctx.runtime_manager->getRuntimeFunction("nova_memory_alloc");
      if (!alloc_func) {
        throw std::runtime_error("未找到nova_memory_alloc函数");
        return -1;
      }

      auto size_val = llvm::ConstantInt::get(ctx.builder->getInt64Ty(), vtable_size);
      auto vtable_ptr = ctx.builder->CreateCall(alloc_func, {size_val}, name + "_vtable");

      auto data_ptr = ctx.builder->CreateCall(
          ctx.runtime_manager->getRuntimeFunction("nova_memory_get_data"),
          {vtable_ptr});
      
      auto byte_ptr = ctx.builder->CreateBitCast(
          data_ptr, llvm::PointerType::get(ctx.builder->getInt8Ty(), 0));

      auto vtable_global = new llvm::GlobalVariable(
          *ctx.module,
          llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(),
                                 0),
          false, // isConstant
          llvm::GlobalValue::ExternalLinkage,
          llvm::Constant::getNullValue(llvm::PointerType::get(
              ctx.runtime_manager->getNovaMemoryBlockType(), 0)),
          name + "_vtable_global");

      ctx.builder->CreateStore(vtable_ptr, vtable_global);
      llvm_vtable = vtable_global;

      /* copy parent class vtable */
      if (parent_class && parent_class->llvm_vtable) {
        auto parent_vtable_ptr = ctx.builder->CreateLoad(
            llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0),
            parent_class->llvm_vtable,
            "parent_vtable_ptr");
        
        size_t parent_vtable_size = parent_class->vtable.size() * sizeof(void*);
        if (parent_vtable_size > 0) {
          auto memcpy_func = ctx.runtime_manager->getRuntimeFunction("nova_memory_copy");
          if (memcpy_func) {
            ctx.builder->CreateCall(memcpy_func, {
                vtable_ptr,
                parent_vtable_ptr,
                llvm::ConstantInt::get(ctx.builder->getInt64Ty(), parent_vtable_size)
            });
          }
        }
      }

      // 遍历所有虚函数，写入或覆盖虚函数表
      for (size_t i = 0; i < vtable.size(); i++) {
        if (parent_class && i < parent_class->vtable.size()) {
          if (parent_class->vtable[i].first != vtable[i].first) {
            throw std::runtime_error("parent class and child class vtable function name mismatch: " + parent_class->vtable[i].first + " != " + vtable[i].first);
          }
          if (parent_class->vtable[i].second == vtable[i].second) {
            continue;
          }
        }

        auto func_info = lookup_func(vtable[i].first);
        if (!func_info) {
          throw std::runtime_error("can't find function: " + vtable[i].first
            + " src_line:" + std::to_string(line) + " file:" + __FILE__ + " line:" + std::to_string(__LINE__));
          continue;
        }
        if (!func_info->llvm_func) {
          throw std::runtime_error("function " + vtable[i].first + " llvm is null"
            + " src_line:" + std::to_string(line) + " file:" + __FILE__ + " line:" + std::to_string(__LINE__));
          continue;
        }

        size_t offset = i * sizeof(void*);
        
        auto value_ptr_ptr = ctx.builder->CreateGEP(
            ctx.builder->getInt8Ty(), byte_ptr,
            llvm::ConstantInt::get(ctx.builder->getInt64Ty(), offset));
            
        auto value_ptr = ctx.builder->CreateBitCast(
            value_ptr_ptr,
            llvm::PointerType::get(
                llvm::PointerType::get(ctx.builder->getInt8Ty(), 0), 0));

        auto func_ptr = ctx.builder->CreateBitCast(
            func_info->llvm_func,
            llvm::PointerType::get(ctx.builder->getInt8Ty(), 0));

        ctx.builder->CreateStore(func_ptr, value_ptr);
      }
    }
  }
  return 0;
}

int StructLiteral::gencode_expr(VarType expected_type, llvm::Value *&value) {
  if (type == VarType::CLASS) {
    print_backtrace();
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "class can't be used as expression", line,
                  __FILE__, __LINE__);
    return -1;
  }

  if (llvm_instance)
  {
    value = llvm_instance;
    return 0;
  }

  std::cout << "Gencode instance: " << name << std::endl;
  size_t total_size = 0;
  /* size of instance */
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

    std::cout << "instance:" << name << " field: " << field.first
              << " type: " << var_type_to_string(field_type) << std::endl;
    total_size += get_type_align(field_type);
  }
  if (total_size == 0)
    throw std::runtime_error("instance size is 0");
  
  /* first: allocate instance memory */
  auto nova_memory_alloc_func =
      ctx.runtime_manager->getRuntimeFunction("nova_memory_alloc");

  llvm::Value *size_val =
      llvm::ConstantInt::get(ctx.builder->getInt64Ty(), total_size);
  llvm::Value *struct_ptr =
      ctx.builder->CreateCall(nova_memory_alloc_func, {size_val}, "struct_obj");

  value = struct_ptr;

  /* second: generate class, function and attributes, relabel instance memory */
  auto class_info = lookup_struct(name);
  if (class_info) {
    if (class_info->node->gencode_stmt() != 0) {
      return -1;
    }
  } else {
    throw std::runtime_error("class not found: " + name);
  }

  /* third: generate instance fields */
  std::vector<std::pair<std::string, std::pair<VarType, llvm::Value *>>>
      field_data;
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

    llvm::Value *field_value = nullptr;
    if (field.second->gencode_expr(field_type, field_value) != 0) {
      return -1;
    }
    field_data.push_back({field.first, {field_type, field_value}});
  }

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
    if (field_value == nullptr) {
      ctx.print_errors();
      throw std::runtime_error("llvm_value:" + field_info.first + " type:" + var_type_to_string(field_type) + " is nullptr");
    }

      // 根据字段类型确定存储方式
      switch (field_type) {
      case VarType::INT: {
        auto value_ptr_ptr = ctx.builder->CreateGEP(
            ctx.builder->getInt8Ty(), byte_ptr,
            llvm::ConstantInt::get(ctx.builder->getInt64Ty(), offset));
        auto value_ptr = ctx.builder->CreateBitCast(
            value_ptr_ptr,
            llvm::PointerType::get(ctx.builder->getInt64Ty(), 0));

        ctx.builder->CreateStore(field_value, value_ptr);
        break;
      }
      case VarType::FLOAT: {
        auto value_ptr_ptr = ctx.builder->CreateGEP(
            ctx.builder->getInt8Ty(), byte_ptr,
            llvm::ConstantInt::get(ctx.builder->getInt64Ty(), offset));
        auto value_ptr = ctx.builder->CreateBitCast(
            value_ptr_ptr,
            llvm::PointerType::get(ctx.builder->getDoubleTy(), 0));

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
        // std::cout << "gen instance function: " << field_info.first << "
        // func_ptr: " << func_ptr << " value_ptr: " << value_ptr << std::endl;
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
                      "不支持的结构体字段类型: " +
                          var_type_to_string(field_type),
                      line, __FILE__, __LINE__);
        return -1;
      }

    // 更新偏移量
    offset += get_type_align(field_type);
  }

  llvm_instance = value;
  return 0;
}