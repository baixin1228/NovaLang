#include "Context.h"
#include <string>
#include "Common.h"
#include "Function.h"  // 添加 Function 类的头文件

int Context::add_var_info(const std::string& name, int line) {
    // 检查变量是否已存在且定义行号小于等于当前行
    auto it = var_infos.find(name);
    if (it != var_infos.end() && it->second.line <= line) {
        return -1;
    }

    var_infos[name] = {.line = line};
    return 0;
}

VarInfo& Context::lookup_var_info(const std::string &name, int line) {
  auto it = var_infos.find(name);
  if (it != var_infos.end()) {
    if (it->second.line <= line) {
      return it->second;
    } else {
#ifdef DEBUG
            std::cout << "get global type failed: " << name << " line: " << line << " " << it->second.line << std::endl;
            for (auto &[name, type] : var_infos) {
                std::cout << name << " line: " << type.line << " type: " << var_type_to_string(type.type) << std::endl;
            }
#endif
        }
    }
    throw std::runtime_error("variable not found: " + name +
                             " line: " + std::to_string(line) + " file: " + __FILE__ + " line: " + std::to_string(__LINE__));
}

int Context::add_func_info(const std::string &name) {
  if (func_infos.find(name) == func_infos.end()) {
    func_infos[name] = {};
    return 0;
  }
  return -1;
}

FuncInfo& Context::get_func_info(const std::string &name) {
  if (func_infos.find(name) == func_infos.end()) {
    throw std::runtime_error("function not found: " + name + " file: " + __FILE__ + " line: " + std::to_string(__LINE__));
  }
  return func_infos[name];
}

bool Context::has_func(const std::string& name) const {
    return func_infos.find(name) != func_infos.end();
}

const std::shared_ptr<ASTNode>&
Context::get_func(const std::string &name) const {
  static const std::shared_ptr<ASTNode> null_ptr;
  auto it = func_infos.find(name);
  if (it != func_infos.end()) {
    return ast[it->second.ast_index];
  }
  return null_ptr;
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

void Context::add_global_struct(const std::string& name, std::shared_ptr<ASTNode> node) {
    if (global_structs.find(name) == global_structs.end()) {
        global_structs[name] = node;
    }
}

std::shared_ptr<ASTNode> Context::lookup_global_struct(const std::string& name) const {
    auto it = global_structs.find(name);
    return it != global_structs.end() ? it->second : nullptr;
}
