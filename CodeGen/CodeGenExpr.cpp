#include "CodeGen.h"
#include "IntLiteral.h"
#include "FloatLiteral.h"
#include "BoolLiteral.h"
#include "StringLiteral.h"
#include "Variable.h"
#include "BinOp.h"
#include "UnaryOp.h"
#include "Call.h"
#include <llvm-14/llvm/IR/Value.h>

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
            default:
                throw std::runtime_error("未知变量类型: " + var->name + " code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
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
    throw std::runtime_error("未知表达式类型 code:" + std::to_string(node.line) + " line:" + std::to_string(__LINE__));
    return VarType::NONE;
} 