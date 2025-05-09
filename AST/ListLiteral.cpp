#include "ListLiteral.h"
#include "TypeChecker.h"

int ListLiteral::visit_stmt() {
    type = VarType::LIST;
    return 0;
}

int ListLiteral::visit_expr(std::shared_ptr<ASTNode> &expr_ret) {
    if (elements.empty()) {
        type = VarType::LIST;
        return 0;
    }
    
    // 确定元素类型
    VarType first_element_type;
    std::shared_ptr<ASTNode> first_element_ast;
    if (elements[0]->visit_expr(first_element_ast) == -1) {
        return -1;
    }

    element_type = first_element_ast->type;

    // 确保所有元素都有相同的类型
    for (size_t i = 1; i < elements.size(); ++i) {
        std::shared_ptr<ASTNode> element_ast;
        if (elements[i]->visit_expr(element_ast) == -1) {
            return -1;
        }
        
        if (element_ast->type != element_type) {
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                         "列表元素类型不一致，期望：" + var_type_to_string(element_type) + 
                         "，得到：" + var_type_to_string(element_ast->type),
                         line, __FILE__, __LINE__);
            return -1;
        }
    }
    
    type = VarType::LIST;
    expr_ret = shared_from_this();
    return 0;
}

int ListLiteral::gencode_stmt() { return 0; }

int ListLiteral::gencode_expr(VarType expected_type, llvm::Value *&ret_value) {
  // 计算所需内存大小 - 需要存储元素数量和元素值
  size_t total_size = sizeof(size_t); // 存储元素数量

  // 创建元素数据的数据结构，用于后续写入内存
  std::vector<std::pair<VarType, llvm::Value *>> element_data;

  // 遍历元素计算总大小
  for (const auto &element : elements) {
    std::shared_ptr<ASTNode> element_ast;
    int ret = element->visit_expr(element_ast);
    if (ret == -1) {
      return -1;
    }
    VarType element_type = element_ast->type;

    // 计算元素值所需空间
    total_size += get_type_align(element_type);

    // 准备元素值
    llvm::Value *element_value = nullptr;
    if (element->gencode_expr(element_type, element_value) != 0) {
      return -1;
    }
    element_data.push_back({element_type, element_value});
  }

  // 使用nova_memory_alloc创建一个列表对象，分配所需内存
  auto nova_memory_alloc_func = ctx.runtime_manager->getRuntimeFunction("nova_memory_alloc");
  llvm::Value *size_val = llvm::ConstantInt::get(ctx.builder->getInt64Ty(), total_size);
  llvm::Value *list_ptr = ctx.builder->CreateCall(nova_memory_alloc_func, {size_val}, "list_obj");

  // 获取数据区域指针
  auto data_ptr = ctx.builder->CreateCall(
      ctx.runtime_manager->getRuntimeFunction("nova_memory_get_data"),
      {list_ptr});
  auto byte_ptr = ctx.builder->CreateBitCast(
      data_ptr, llvm::PointerType::get(ctx.builder->getInt8Ty(), 0));

  // 存储元素数量
  auto count_ptr = ctx.builder->CreateBitCast(
      byte_ptr, llvm::PointerType::get(ctx.builder->getInt64Ty(), 0));
  ctx.builder->CreateStore(
      llvm::ConstantInt::get(ctx.builder->getInt64Ty(), elements.size()),
      count_ptr);

  // 当前偏移量，从元素数量之后开始
  size_t offset = sizeof(size_t);

  // 为每个元素设置值
  for (const auto &element_info : element_data) {
    VarType element_type = element_info.first;
    llvm::Value *element_value = element_info.second;

    // 根据元素类型确定存储方式
    switch (element_type) {
    case VarType::INT: {
      auto value_ptr_ptr = ctx.builder->CreateGEP(
          ctx.builder->getInt8Ty(), byte_ptr,
          llvm::ConstantInt::get(ctx.builder->getInt64Ty(), offset));
      auto value_ptr = ctx.builder->CreateBitCast(
          value_ptr_ptr, llvm::PointerType::get(ctx.builder->getInt64Ty(), 0));
      ctx.builder->CreateStore(element_value, value_ptr);
      break;
    }
    case VarType::FLOAT: {
      auto value_ptr_ptr = ctx.builder->CreateGEP(
          ctx.builder->getInt8Ty(), byte_ptr,
          llvm::ConstantInt::get(ctx.builder->getInt64Ty(), offset));
      auto value_ptr = ctx.builder->CreateBitCast(
          value_ptr_ptr, llvm::PointerType::get(ctx.builder->getDoubleTy(), 0));
      ctx.builder->CreateStore(element_value, value_ptr);
      break;
    }
    case VarType::BOOL: {
      auto value_ptr_ptr = ctx.builder->CreateGEP(
          ctx.builder->getInt8Ty(), byte_ptr,
          llvm::ConstantInt::get(ctx.builder->getInt64Ty(), offset));
      auto value_ptr = ctx.builder->CreateBitCast(
          value_ptr_ptr, llvm::PointerType::get(ctx.builder->getInt1Ty(), 0));
      ctx.builder->CreateStore(element_value, value_ptr);
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
              llvm::PointerType::get(ctx.runtime_manager->getNovaMemoryBlockType(), 0),
              0));
      ctx.builder->CreateStore(element_value, value_ptr);

      // 对于引用类型，需要增加引用计数
      auto retain_func = ctx.runtime_manager->getRuntimeFunction("nova_memory_retain");
      ctx.builder->CreateCall(retain_func, {element_value});
      break;
    }
    default:
      ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "不支持的列表元素类型: " + var_type_to_string(element_type),
                    line, __FILE__, __LINE__);
      return -1;
    }

    // 更新偏移量
    offset += get_type_align(element_type);
  }

  ret_value = list_ptr;
  return 0;
}