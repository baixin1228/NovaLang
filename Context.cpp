#include "Context.h"
#include <string>
#include "Function.h"  // 添加 Function 类的头文件

int Context::add_var_type(const std::string& name, int line, VarType type) {
    // 检查变量是否已存在且定义行号小于等于当前行
    auto it = var_types.find(name);
    if (it != var_types.end() && it->second.first <= line) {
#ifdef DEBUG
      std::cout << "global type already exists: " << name << " line: " << line << std::endl;
#endif
        return -1;
    }

#ifdef DEBUG
    std::cout << "set global type: " << name << " line: " << line
              << " type: " << var_type_to_string(type) << std::endl;
#endif
    var_types[name] = {line, type};
    return 0;
}

VarType Context::get_var_type(const std::string& name, int line) {
    auto it = var_types.find(name);
    if (it != var_types.end()) {
        if (it->second.first <= line) {
// #ifdef DEBUG
//             std::cout << "get global type: " << name << " line: " << line << " type: " << var_type_to_string(it->second.second) << " found" << std::endl;
// #endif
            return it->second.second;
        } else {
#ifdef DEBUG
            std::cout << "get global type failed: " << name << " line: " << line << " " << it->second.first << std::endl;
            for (auto &[name, type] : var_types) {
                std::cout << name << " line: " << type.first << " type: " << var_type_to_string(type.second) << std::endl;
            }
#endif
        }
    }
    return VarType::NONE;
}

void Context::set_func_type(const std::string& name,
                          const std::vector<VarType>& param_types,
                          VarType return_type) {
    func_types[name] = std::make_pair(param_types, return_type);
}

void Context::set_func_return_type(const std::string &name,
                                    VarType return_type) {
  func_types[name].second = return_type;
}

std::pair<std::vector<VarType>, VarType>&
Context::get_func_type(const std::string &name) {
  return func_types[name];
}

bool Context::has_func(const std::string& name) const {
    return func_types.find(name) != func_types.end();
}

const std::shared_ptr<ASTNode>&
Context::get_func(const std::string &name) const {
  static const std::shared_ptr<ASTNode> null_ptr;
  auto it = funcline.find(name);
  if (it != funcline.end()) {
    return ast[it->second];
  }
  return null_ptr;
}

void Context::set_funcline(const std::string& name, uint64_t value) {
    funcline[name] = value;
}

std::optional<uint64_t> Context::get_funcline(const std::string& name) const {
    auto it = funcline.find(name);
    if (it != funcline.end()) {
        return it->second;
    }
    return std::nullopt;
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

void Context::add_llvm_symbol(const std::string& name, llvm::Value* value) {
    std::cout << "add global llvm symbol: " << name << std::endl;
    global_symbols[name] = value;
}

llvm::Value* Context::lookup_llvm_symbol(const std::string& name) const {
    auto it = global_symbols.find(name);
    return it != global_symbols.end() ? it->second : nullptr;
}

// int Context::erase_llvm_symbol(const std::string& name) {
//     auto it = global_symbols.find(name);
//     if (it != global_symbols.end()) {
//         global_symbols.erase(it);
//         return 0;
//     }
//     return -1;
// } 

void Context::add_global_struct(const std::string& name, std::shared_ptr<ASTNode> node) {
    if (global_structs.find(name) == global_structs.end()) {
        global_structs[name] = node;
    }
}

std::shared_ptr<ASTNode> Context::lookup_global_struct(const std::string& name) const {
    auto it = global_structs.find(name);
    return it != global_structs.end() ? it->second : nullptr;
}
