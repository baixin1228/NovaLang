#include "Context.h"
#include <string>
#include "Common.h"
#include "Variable.h"

int Context::add_global_var(const std::string &name,
                            std::shared_ptr<VarInfo> node) {
  // 检查变量是否已存在且定义行号小于等于当前行
  auto it = global_vars.find(name);
  if (it != global_vars.end() && it->second->line <= node->line) {
#ifdef DEBUG
  std::cout << "-- add global var failed: " << name << " line:" << node->line << std::endl;
#endif
        return -1; 
    }

    if (!node)
    {
        throw std::runtime_error("add global var failed: " + name + " line:" + std::to_string(node->line) + " file:" + std::string(__FILE__) + " line:" + std::to_string(__LINE__));
        return -1;
    }
      global_vars[name] = node;
#ifdef DEBUG
  std::cout << "-- add global var: " << name << " " << node->line << std::endl;
#endif
    return 0;
}

std::shared_ptr<VarInfo> Context::lookup_global_var(const std::string &name,
                                                    int line) {
  auto it = global_vars.find(name);
  if (it != global_vars.end()) {
    if (line == -1 || it->second->line <= line) {
      if (!it->second) {
        throw std::runtime_error("lookup global var failed: " + name +
                                 " line:" + std::to_string(line) +
                                 " file:" + std::string(__FILE__) +
                                 " line:" + std::to_string(__LINE__));
        return nullptr;
      }
      return it->second;
    } else {
#ifdef DEBUG
      std::cout << "-- global var can't access: " << name << " line: " << line
                << std::endl;
#endif
    }
  } else {
#ifdef DEBUG
      std::cout << "-- get global var failed: " << name << " line: " << line
                << std::endl;
      // for (auto &[name, node] : global_vars) {
      //   std::cout << "    " << name
      //             << "-- available global var is line: " << node->line
      //             << " type: " << var_type_to_string(node->type) << std::endl;
      // }
#endif
    }
    return nullptr;
}

int Context::add_global_func(const std::string &name, std::shared_ptr<FuncInfo> node) {
  if (global_funcs.find(name) == global_funcs.end()) {
    global_funcs[name] = node;
    return 0;
  }
  return -1;
}

std::shared_ptr<FuncInfo> Context::lookup_global_func(const std::string &name) {
  if(name.empty()) {
    return nullptr;
  }
  if (global_funcs.find(name) == global_funcs.end()) {
#ifdef DEBUG
    std::cout << "-- lookup global func failed: " << name << std::endl;
#endif
    return nullptr;
  }
#ifdef DEBUG
  std::cout << "-- lookup global func: " << name << std::endl;
#endif
  return global_funcs[name];
}

int Context::add_global_struct(const std::string& name, std::shared_ptr<ClassInfo> node) {
    if (global_structs.find(name) == global_structs.end()) {
        global_structs[name] = node;
#ifdef DEBUG
    std::cout << "add global struct: " << name << std::endl;
#endif
      return 0;
    }
    return -1;
}

std::shared_ptr<ClassInfo> Context::lookup_global_struct(const std::string& name) {
  if (name.empty()) {
    return nullptr;
  }
  if (global_structs.find(name) == global_structs.end()) {
#ifdef DEBUG
    std::cout << "-- lookup global struct failed: " << name << std::endl;
#endif
    return nullptr;
  }
#ifdef DEBUG
  std::cout << "-- lookup global struct: " << name << std::endl;
#endif
  return global_structs[name];
}

void Context::add_ast_node(std::shared_ptr<ASTNode> node) {
    ast.push_back(std::move(node));
}

std::vector<std::shared_ptr<ASTNode>>& Context::get_ast() {
    return ast;
}

void Context::add_error(ErrorHandler::ErrorLevel level, const std::string& msg, int line, const char* file, int call_line) {
    errors.add_error(level, msg, line, file, call_line);
}

void Context::add_error_front(ErrorHandler::ErrorLevel level, const std::string& msg, int line, const char* file, int call_line) {
    errors.add_error_front(level, msg, line, file, call_line);
}

bool Context::has_errors() const {
    return errors.has_errors();
}

void Context::print_errors() const {
    errors.print_errors();
}

std::string Context::generate_local_var_name(const std::string& original_name, const ASTNode* node) const {
    if (!node) return original_name;
    
    // 获取作用域路径和深度
    std::string scope_path = node->get_scope_path();
    int depth = node->get_scope_depth();
    
    // 生成唯一的局部变量名：原始名称_深度_作用域路径
    // 例如：x_2_15_23 表示在深度2，路径.15.23的作用域中的变量x
    return original_name + "_" + std::to_string(depth) + "_" +  scope_path;
}

void Context::set_source_filename(const std::string& filename) {
    source_filename = filename;
}

const std::string& Context::get_source_filename() const {
    return source_filename;
}

llvm::Type* Context::get_llvm_type(VarType type) const {
    switch (type) {
        case VarType::INT:
            return llvm::Type::getInt64Ty(*llvm_context);
        case VarType::FLOAT:
            return llvm::Type::getDoubleTy(*llvm_context);
        case VarType::BOOL:
            return llvm::Type::getInt1Ty(*llvm_context);
        case VarType::STRING:
            return llvm::PointerType::get(llvm::Type::getInt8Ty(*llvm_context), 0); // char*
        case VarType::STRUCT:
        case VarType::DICT:
        case VarType::LIST:
            // For custom types, we use a pointer type
            return llvm::PointerType::get(*llvm_context, 0);
        case VarType::VOID:
        case VarType::NONE:
        default:
            return llvm::Type::getVoidTy(*llvm_context);
    }
}

void Context::push_func_stack(llvm::Function *func, llvm::BasicBlock *block) {
    func_stack.push({func, block});
    builder->SetInsertPoint(block);
    current_function = func;
}

void Context::pop_func_stack() {
  func_stack.pop();
  if (!func_stack.empty()) {
    auto [func, block] = func_stack.top();
    builder->SetInsertPoint(block);
    current_function = func;
  }
}

void Context::update_insert_point(llvm::BasicBlock *block) {
  func_stack.top().second = block;
  builder->SetInsertPoint(block);
}