#include "CodeGen.h"
#include "IntLiteral.h"
#include "FloatLiteral.h"
#include "BoolLiteral.h"
#include "StringLiteral.h"
#include "Variable.h"
#include "BinOp.h"
#include "UnaryOp.h"
#include "Call.h"
#include "AST/StructLiteral.h"
#include "AST/DictLiteral.h"
#include "AST/ListLiteral.h"
#include "AST/StructFieldAccess.h"
#include <llvm-14/llvm/ADT/None.h>
#include <llvm-14/llvm/IR/Value.h>

// 处理结构体字段访问，返回字段值
llvm::Value* CodeGen::handleStructFieldAccess(StructFieldAccess* field_access) {
    // 获取结构体引用
    auto struct_val = visit_expr(*field_access->struct_expr);
    if (!struct_val) {
        throw std::runtime_error("无法访问无效结构体的字段 code:" + 
                                std::to_string(field_access->line) + " line:" + 
                                std::to_string(__LINE__));
        return nullptr;
    }
    
    // 获取字段名和字段类型
    const std::string& field_name = field_access->field_name;
    VarType field_type = visit_expr_type(*field_access);
    
    // 获取结构体数据区域指针
    auto data_ptr = builder.CreateCall(runtime_manager->getRuntimeFunction("nova_memory_get_data"), {struct_val});
    auto byte_ptr = builder.CreateBitCast(data_ptr, llvm::PointerType::get(builder.getInt8Ty(), 0));
    
    // 计算字段偏移量
    size_t field_offset = 0;
    bool found_field = false;
    
    // 从结构体定义中查找字段位置
    // 遍历结构体定义中的字段，计算偏移量
    auto* struct_expr = field_access->struct_expr.get();
    
    // 从原始的结构体字面量中获取字段信息
    auto *var_lit = dynamic_cast<Variable *>(struct_expr);
    auto ast_lit = ctx.lookup_global_struct(var_lit->name);
    if (ast_lit) {
      auto *struct_lit = dynamic_cast<StructLiteral *>(ast_lit.get());
      const auto &fields = struct_lit->fields;
      for (const auto &field : fields) {
        if (field.first == field_name) {
          found_field = true;
          break;
        }
        field_offset += get_type_align(visit_expr_type(*field.second));
      }
    }

    if (!found_field) {
        throw std::runtime_error("结构体不存在字段: " + field_name + " code:" + 
                               std::to_string(field_access->line) + " line:" + 
                               std::to_string(__LINE__));
        return nullptr;
    }
    
    // 根据字段类型和偏移量获取字段值
    llvm::Value* field_ptr_ptr = builder.CreateGEP(builder.getInt8Ty(), byte_ptr, 
        llvm::ConstantInt::get(builder.getInt64Ty(), field_offset));
    
    llvm::Value* field_val = nullptr;
    
    // 根据字段类型加载值
    switch (field_type) {
        case VarType::INT: {
            auto field_ptr = builder.CreateBitCast(field_ptr_ptr, 
                llvm::PointerType::get(builder.getInt64Ty(), 0));
            field_val = builder.CreateLoad(builder.getInt64Ty(), field_ptr, 
                "field_" + field_name);
            break;
        }
        case VarType::FLOAT: {
            auto field_ptr = builder.CreateBitCast(field_ptr_ptr, 
                llvm::PointerType::get(builder.getDoubleTy(), 0));
            field_val = builder.CreateLoad(builder.getDoubleTy(), field_ptr, 
                "field_" + field_name);
            break;
        }
        case VarType::BOOL: {
            auto field_ptr = builder.CreateBitCast(field_ptr_ptr, 
                llvm::PointerType::get(builder.getInt1Ty(), 0));
            field_val = builder.CreateLoad(builder.getInt1Ty(), field_ptr, 
                "field_" + field_name);
            break;
        }
        case VarType::STRING:
        case VarType::STRUCT:
        case VarType::DICT:
        case VarType::LIST: {
            auto field_ptr = builder.CreateBitCast(field_ptr_ptr, 
                llvm::PointerType::get(llvm::PointerType::get(runtime_manager->getNovaMemoryBlockType(), 0), 0));
            field_val = builder.CreateLoad(
                llvm::PointerType::get(runtime_manager->getNovaMemoryBlockType(), 0), 
                field_ptr, 
                "field_" + field_name);
            break;
        }
        default:
            ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                        "不支持的结构体字段类型: " + var_type_to_string(field_type),
                        field_access->line, __FILE__, __LINE__);
            return nullptr;
    }
    
    return field_val;
}

llvm::Value* CodeGen::handleStringLiteral(StringLiteral* str_lit) {
    // 获取原始字符串作为key
    const std::string& str_key = str_lit->get_raw_str();
    
    // 检查字符串池中是否已存在
    auto it = string_pool.find(str_key);
    if (it != string_pool.end()) {
        return it->second;  // 返回已存在的内存指针
    }
    
    // 获取Unicode字符串
    const icu::UnicodeString& unicode_str = str_lit->get_unicode();
    
    // 创建包含Unicode数据的全局常量数组
    llvm::Constant* chars_array = nullptr;
    std::vector<llvm::Constant*> chars;
    for (int i = 0; i < unicode_str.length(); i++) {
        chars.push_back(llvm::ConstantInt::get(builder.getInt16Ty(), unicode_str.charAt(i)));
    }
    
    // 创建UChar数组类型和常量数组
    llvm::ArrayType* char_array_type = llvm::ArrayType::get(builder.getInt16Ty(), chars.size());
    chars_array = llvm::ConstantArray::get(char_array_type, chars);
    
    // 将数组存储为全局变量以便使用
    std::string array_name = "unicode_chars_" + std::to_string(string_pool.size());
    auto global_array = new llvm::GlobalVariable(
        *module,
        char_array_type,
        true, // 是常量
        llvm::GlobalValue::PrivateLinkage,
        chars_array,
        array_name
    );
    
    // 创建指向数组的指针
    llvm::Value* array_ptr = builder.CreateBitCast(
        global_array, 
        llvm::PointerType::get(builder.getInt16Ty(), 0),
        "unicode_data_ptr"
    );
    
    // 使用运行时库函数创建Unicode字符串
    auto create_func = runtime_manager->getRuntimeFunction("create_string_from_chars");
    llvm::Value* length = llvm::ConstantInt::get(builder.getInt32Ty(), unicode_str.length());
    llvm::Value* memory_block = builder.CreateCall(
        create_func,
        {array_ptr, length},
        "create_string"
    );
    
    // 将指针存入字符串池
    string_pool[str_key] = memory_block;
    
    return memory_block;
}

llvm::Value* CodeGen::visit_expr(ASTNode& node, VarType expected_type) {
    if (auto* str_lit = dynamic_cast<StringLiteral*>(&node)) {
        return handleStringLiteral(str_lit);
    }
    if (auto* int_lit = dynamic_cast<IntLiteral*>(&node)) {
      if (expected_type != VarType::NONE && expected_type == VarType::FLOAT)
      {
        // 隐式类型转换
        return builder.CreateSIToFP(builder.getInt64(int_lit->value),
                                    builder.getDoubleTy());
      }
      return builder.getInt64(int_lit->value);
    }
    if (auto* float_lit = dynamic_cast<FloatLiteral*>(&node)) {
        return llvm::ConstantFP::get(builder.getDoubleTy(), float_lit->value);
    }
    if (auto* bool_lit = dynamic_cast<BoolLiteral*>(&node)) {
        return builder.getInt1(bool_lit->value);
    }
    if (auto* var = dynamic_cast<Variable*>(&node)) {
        auto ptr = var->lookup_llvm_symbol(var->name);
        if (!ptr) {
            throw std::runtime_error("未定义的变量: " + var->name + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
            return nullptr;
        }

        VarType type = var->lookup_var_type(var->name);
        switch (type) {
            case VarType::STRING: {
                auto memory_block_ptr_type = llvm::PointerType::get(runtime_manager->getNovaMemoryBlockType(), 0);
                auto load = builder.CreateLoad(memory_block_ptr_type, ptr, var->name + "_load");
                load->setAlignment(llvm::Align(get_type_align(type)));
                return load;
            }
            case VarType::INT: {
                auto load = builder.CreateLoad(builder.getInt64Ty(), ptr, var->name + "_load");
                load->setAlignment(llvm::Align(get_type_align(type)));
                if (expected_type != VarType::NONE &&
                    expected_type == VarType::FLOAT) {
                    //隐式类型转换
                  return builder.CreateSIToFP(load, builder.getDoubleTy());
                }
                return load;
            }
            case VarType::FLOAT: {
                auto load = builder.CreateLoad(builder.getDoubleTy(), ptr, var->name + "_load");
                load->setAlignment(llvm::Align(get_type_align(type)));
                return load;
            }
            case VarType::BOOL: {
                auto load = builder.CreateLoad(builder.getInt1Ty(), ptr, var->name + "_load");
                load->setAlignment(llvm::Align(get_type_align(type)));
                return load;
            }
            case VarType::STRUCT: {
                // 对于结构体变量，加载结构体指针
                auto memory_block_ptr_type = llvm::PointerType::get(runtime_manager->getNovaMemoryBlockType(), 0);
                auto load = builder.CreateLoad(memory_block_ptr_type, ptr, var->name + "_load");
                load->setAlignment(llvm::Align(get_type_align(type)));
                return load;
            }
            case VarType::DICT:
            case VarType::LIST: {
                // 其他复合类型，加载内存块指针
                auto memory_block_ptr_type = llvm::PointerType::get(runtime_manager->getNovaMemoryBlockType(), 0);
                auto load = builder.CreateLoad(memory_block_ptr_type, ptr, var->name + "_load");
                load->setAlignment(llvm::Align(get_type_align(type)));
                return load;
            }
            default:
                throw std::runtime_error("加载数据失败，未知变量类型: " + var->name + " code:" + std::to_string(node.line) + " file:" + __FILE__ + " line:" + std::to_string(__LINE__));
                return nullptr;
        }
    }
    if (auto* binop = dynamic_cast<BinOp*>(&node)) {
      VarType type = visit_expr_type(*binop->left);
      VarType left_expected_type = VarType::NONE;
      VarType right_expected_type = VarType::NONE;
      auto left_type = visit_expr_type(*binop->left);
      auto right_type = visit_expr_type(*binop->right);
      
      // 检查是否是字符串连接操作
      bool is_string_concat = binop->op == "+" && 
          (left_type == VarType::STRING || right_type == VarType::STRING);
      
      // 如果不是字符串连接，按原逻辑处理类型转换
      if (!is_string_concat) {
        if (expected_type != VarType::NONE) {
          left_expected_type = expected_type;
          right_expected_type = expected_type;
          type = expected_type;
        }
        if (left_type != right_type) {
          if (left_type == VarType::FLOAT) {
            right_expected_type = VarType::FLOAT;
          }
          if (right_type == VarType::FLOAT) {
            left_expected_type = VarType::FLOAT;
          }
          type = VarType::FLOAT;
        }
        if (binop->op == "/") {
          left_expected_type = VarType::FLOAT;
          right_expected_type = VarType::FLOAT;
          type = VarType::FLOAT;
        }
      } else {
        // 对于字符串连接，设置返回类型为STRING
        type = VarType::STRING;
      }
      
      auto left = visit_expr(*binop->left, left_expected_type);
      auto right = visit_expr(*binop->right, right_expected_type);
      
      if (left == nullptr || right == nullptr) {
        throw std::runtime_error(
            "未知表达式 code:" + std::to_string(node.line) +
            " line:" + std::to_string(__LINE__));
        return nullptr;
      }
      
      if (binop->op == "+") {
        // 处理字符串连接
        if (is_string_concat) {
          // 如果左操作数不是字符串，需要将其转换为字符串
          if (left_type != VarType::STRING) {
            // 创建一个临时字符串值
            // 这里需要根据实际情况实现将不同类型转换为字符串的逻辑
            // 目前简单处理：只支持字符串+字符串
            throw std::runtime_error("不支持的操作: 非字符串 + 字符串 code:" + 
                                    std::to_string(node.line) + " line:" + 
                                    std::to_string(__LINE__));
          }
          
          // 如果右操作数不是字符串，需要将其转换为字符串
          if (right_type != VarType::STRING) {
            // 同上，需要实现类型转换
            throw std::runtime_error("不支持的操作: 字符串 + 非字符串 code:" + 
                                    std::to_string(node.line) + " line:" + 
                                    std::to_string(__LINE__));
          }
          
          // 两者都是字符串，调用运行时库进行连接
          auto concat_func = runtime_manager->getRuntimeFunction("concat_strings");
          if (!concat_func) {
            throw std::runtime_error("无法获取字符串连接函数 code:" + 
                                    std::to_string(node.line) + " line:" + 
                                    std::to_string(__LINE__));
          }
          
          // 调用concat_strings函数连接字符串
          return builder.CreateCall(concat_func, {left, right}, "str_concat");
        }
        
        // 非字符串加法处理
        return type == VarType::INT ?
            builder.CreateAdd(left, right, "add") :
            builder.CreateFAdd(left, right, "fadd");
      }
      if (binop->op == "-") {
            return type == VarType::INT ?
                builder.CreateSub(left, right, "sub") :
                builder.CreateFSub(left, right, "fsub");
        }
        if (binop->op == "*") {
            return type == VarType::INT ?
                builder.CreateMul(left, right, "mul") :
                builder.CreateFMul(left, right, "fmul");
        }
        if (binop->op == "/") {
            return builder.CreateFDiv(left, right, "fdiv");
        }
        if (binop->op == "//") {
          return builder.CreateSDiv(left, right, "sdiv");
        }
        if (binop->op == "==") {
            if (type == VarType::INT) {
                return builder.CreateICmpEQ(left, right, "eq");
            } else if (type == VarType::FLOAT) {
                return builder.CreateFCmpOEQ(left, right, "feq");
            } else {
                return builder.CreateICmpEQ(left, right, "eq");
            }
        }
        if (binop->op == "<") {
            return type == VarType::INT ?
                builder.CreateICmpSLT(left, right, "cmp") :
                builder.CreateFCmpOLT(left, right, "fcmp");
        }
        if (binop->op == ">") {
            return type == VarType::INT ?
                builder.CreateICmpSGT(left, right, "cmp") :
                builder.CreateFCmpOGT(left, right, "fcmp");
        }
        if (binop->op == ">=") {
            return type == VarType::INT ?
                builder.CreateICmpSGE(left, right, "cmp") :
                builder.CreateFCmpOGE(left, right, "fcmp");
        }
        if (binop->op == "and") {
            return builder.CreateAnd(left, right, "and");
        }
        if (binop->op == "or") {
            return builder.CreateOr(left, right, "or");
        }
        throw std::runtime_error("未知运算符: " + binop->op + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
        return nullptr;
    }
    if (auto* unary = dynamic_cast<UnaryOp*>(&node)) {
        auto expr = visit_expr(*unary->expr);
        if (unary->op == "not") {
            return builder.CreateNot(expr, "not");
        }
        throw std::runtime_error("未知一元运算符: " + unary->op + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
        return nullptr;
    }
    if (auto* call = dynamic_cast<Call*>(&node)) {
        auto func = functions[call->name];
        std::vector<llvm::Value*> args;
        for (auto& arg : call->args) {
            args.push_back(visit_expr(*arg));
        }
        return builder.CreateCall(func, args, "call");
    }
    if (auto* struct_lit = dynamic_cast<StructLiteral*>(&node)) {
        return handleStructLiteral(struct_lit);
    }
    if (auto* dict_lit = dynamic_cast<DictLiteral*>(&node)) {
        // 字典实现将在后续完成，目前简单返回null指针
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "字典字面量暂不支持代码生成",
                    node.line, __FILE__, __LINE__);
        return llvm::ConstantPointerNull::get(
            llvm::PointerType::get(builder.getInt8Ty(), 0));
    }
    if (auto* list_lit = dynamic_cast<ListLiteral*>(&node)) {
        // 列表实现将在后续完成，目前简单返回null指针
        ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                    "列表字面量暂不支持代码生成",
                    node.line, __FILE__, __LINE__);
        return llvm::ConstantPointerNull::get(
            llvm::PointerType::get(builder.getInt8Ty(), 0));
    }
    if (auto* field_access = dynamic_cast<StructFieldAccess*>(&node)) {
        return handleStructFieldAccess(field_access);
    }
    throw std::runtime_error("未知表达式 code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
    return nullptr;
}

VarType CodeGen::visit_expr_type(ASTNode& node) {
    if (auto* str_lit = dynamic_cast<StringLiteral*>(&node)) {
        return VarType::STRING;
    }
    if (auto* int_lit = dynamic_cast<IntLiteral*>(&node)) {
        return VarType::INT;
    }
    if (auto* float_lit = dynamic_cast<FloatLiteral*>(&node)) {
        return VarType::FLOAT;
    }
    if (auto* bool_lit = dynamic_cast<BoolLiteral*>(&node)) {
        return VarType::BOOL;
    }
    if (auto* var = dynamic_cast<Variable*>(&node)) {
      VarType type = var->lookup_var_type(var->name);
      if (type == VarType::NONE) {
        throw std::runtime_error("未定义的变量: " + var->name + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
        return VarType::NONE;
        }
        return type;
    }
    if (auto* binop = dynamic_cast<BinOp*>(&node)) {
      VarType left_type = visit_expr_type(*binop->left);
      VarType right_type = visit_expr_type(*binop->right);

      if (binop->op == "==" || binop->op == "<" || binop->op == ">" ||
          binop->op == ">=") {
        return VarType::BOOL;
      }
      if (binop->op == "and" || binop->op == "or") {
        return VarType::BOOL;
      }
      // 处理字符串连接
      if (binop->op == "+" && (left_type == VarType::STRING || right_type == VarType::STRING)) {
        return VarType::STRING;
      }
      return left_type;
    }
    if (auto* unary = dynamic_cast<UnaryOp*>(&node)) {
        if (unary->op == "not") return VarType::BOOL;
        throw std::runtime_error("未知一元运算符类型 code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
        return VarType::NONE;
    }
    if (auto* call = dynamic_cast<Call*>(&node)) {
      return ctx.get_func_type(call->name).second;
    }
    if (auto* struct_lit = dynamic_cast<StructLiteral*>(&node)) {
        return VarType::STRUCT;
    }
    if (auto* dict_lit = dynamic_cast<DictLiteral*>(&node)) {
        return VarType::DICT;
    }
    if (auto* list_lit = dynamic_cast<ListLiteral*>(&node)) {
        return VarType::LIST;
    }
    if (auto* field_access = dynamic_cast<StructFieldAccess*>(&node)) {
        auto* var_lit = dynamic_cast<Variable*>(field_access->struct_expr.get());
        auto struct_var = ctx.lookup_global_struct(var_lit->name);
        if (!struct_var) {
            throw std::runtime_error("未定义的结构体: " + var_lit->name + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
            return VarType::NONE;
        }
        auto* struct_lit = dynamic_cast<StructLiteral*>(struct_var.get());
        auto filed = struct_lit->fields.find(field_access->field_name);
        if (filed == struct_lit->fields.end()) {
            throw std::runtime_error("未定义的结构体字段: " + field_access->field_name + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
            return VarType::NONE;
        }
        auto field_type = visit_expr_type(*filed->second);
        // 结构体字段的类型就是结构体类型
        return field_type;
    }
    throw std::runtime_error("未知表达式类型 code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
    return VarType::NONE;
}

// 处理结构体字面量，使用nova_memory_alloc分配内存并初始化结构体
llvm::Value* CodeGen::handleStructLiteral(StructLiteral* struct_lit) {
    // 获取结构体字段
    const auto& fields = struct_lit->fields;
    
    // 计算所需内存大小 - 仅需存储字段值
    size_t total_size = 0;
    
    // 创建字段映射的数据结构，用于后续写入内存
    std::vector<std::pair<std::string, std::pair<VarType, llvm::Value*>>> field_data;
    
    // 遍历字段计算总大小
    for (const auto& field : fields) {
        VarType field_type = visit_expr_type(*field.second);
        // 计算字段值所需空间
        total_size += get_type_align(field_type);
        
        // 准备字段值
        auto field_value = visit_expr(*field.second);
        field_data.push_back({field.first, {field_type, field_value}});
    }
    
    // 使用nova_memory_alloc创建一个结构体对象，分配所需内存
    auto nova_memory_alloc_func = runtime_manager->getRuntimeFunction("nova_memory_alloc");
    // 确保分配至少有1字节空间
    if (total_size == 0) total_size = 1;
    llvm::Value* size_val = llvm::ConstantInt::get(builder.getInt64Ty(), total_size);
    llvm::Value* struct_ptr = builder.CreateCall(
        nova_memory_alloc_func,
        {size_val},
        "struct_obj"
    );
    
    // 获取数据区域指针
    auto data_ptr = builder.CreateCall(runtime_manager->getRuntimeFunction("nova_memory_get_data"), {struct_ptr});
    auto byte_ptr = builder.CreateBitCast(data_ptr, llvm::PointerType::get(builder.getInt8Ty(), 0));
    
    // 当前偏移量
    size_t offset = 0;
    
    // 为每个字段设置值
    for (const auto& field_info : field_data) {
        VarType field_type = field_info.second.first;
        llvm::Value* field_value = field_info.second.second;
        
        // 根据字段类型确定存储方式
        switch (field_type) {
            case VarType::INT: {
                auto value_ptr_ptr = builder.CreateGEP(builder.getInt8Ty(), byte_ptr, 
                    llvm::ConstantInt::get(builder.getInt64Ty(), offset));
                auto value_ptr = builder.CreateBitCast(value_ptr_ptr, 
                    llvm::PointerType::get(builder.getInt64Ty(), 0));
                
                builder.CreateStore(field_value, value_ptr);
                break;
            }
            case VarType::FLOAT: {
                auto value_ptr_ptr = builder.CreateGEP(builder.getInt8Ty(), byte_ptr, 
                    llvm::ConstantInt::get(builder.getInt64Ty(), offset));
                auto value_ptr = builder.CreateBitCast(value_ptr_ptr, 
                    llvm::PointerType::get(builder.getDoubleTy(), 0));
                
                builder.CreateStore(field_value, value_ptr);
                break;
            }
            case VarType::BOOL: {
                auto value_ptr_ptr = builder.CreateGEP(builder.getInt8Ty(), byte_ptr, 
                    llvm::ConstantInt::get(builder.getInt64Ty(), offset));
                auto value_ptr = builder.CreateBitCast(value_ptr_ptr, 
                    llvm::PointerType::get(builder.getInt1Ty(), 0));
                
                builder.CreateStore(field_value, value_ptr);
                break;
            }
            case VarType::STRING:
            case VarType::STRUCT:
            case VarType::DICT:
            case VarType::LIST: {
                auto value_ptr_ptr = builder.CreateGEP(builder.getInt8Ty(), byte_ptr, 
                    llvm::ConstantInt::get(builder.getInt64Ty(), offset));
                auto value_ptr = builder.CreateBitCast(value_ptr_ptr, 
                    llvm::PointerType::get(llvm::PointerType::get(runtime_manager->getNovaMemoryBlockType(), 0), 0));
                
                builder.CreateStore(field_value, value_ptr);
                
                // 对于引用类型，需要增加引用计数
                auto retain_func = runtime_manager->getRuntimeFunction("nova_memory_retain");
                builder.CreateCall(retain_func, {field_value});
                break;
            }
            default:
                ctx.add_error(ErrorHandler::ErrorLevel::TYPE,
                            "不支持的结构体字段类型: " + var_type_to_string(field_type),
                            struct_lit->line, __FILE__, __LINE__);
                break;
        }
        
        // 更新偏移量
        offset += get_type_align(field_type);
    }

    return struct_ptr;
}