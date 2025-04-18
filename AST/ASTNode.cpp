#include "ASTNode.h"
#include "Context.h"
#include "Return.h"
#include <iostream>

ASTNode::ASTNode(Context &ctx, int ln, bool is_scope)
    : line(ln), parent(nullptr), ctx(ctx), is_scope(is_scope) {}

void ASTNode::set_parent(ASTNode *p) { parent = p; }

/* 添加变量到最近的作用域，或者全局作用域 */
int ASTNode::add_var_type(const std::string &name, VarType type, bool force) {
  // 如果是全局变量，直接在全局作用域中设置
  if (is_global_var(name)) {
    if (force) {
      throw std::runtime_error("global already exists: " + name +
                               " source_line: " + std::to_string(line) + " file: " + __FILE__ + " line: " + std::to_string(__LINE__));
    }
    return ctx.add_var_type(name, line, type);
  }

  // 如果是作用域节点，在当前节点存储变量
  if (is_scope) {
    // 检查变量是否已存在
    if (var_types.find(name) != var_types.end()) {
#ifdef DEBUG
      std::cout << "variable already exists: " << name << " line: " << line
                << " type: " << var_type_to_string(var_types[name])
                << std::endl;
#endif
      return -1;
    }
    var_types[name] = type;
#ifdef DEBUG
    std::cout << "add variable: " << name << " line: " << line << " type: " << var_type_to_string(type) << std::endl;
#endif
    return 0;
  }

  // 如果有父节点，将变量添加到最近的作用域
  if (parent) {
    return parent->add_var_type(name, type, force);
  }

  // 如果没有找到作用域父节点，添加到全局作用域
  return ctx.add_var_type(name, line, type);
}

VarType ASTNode::lookup_var_type(const std::string &name) {
  if (name.empty()) {
    return VarType::NONE;
  }

  // 1. 在当前作用域查找
  if (is_scope) {
    auto type = var_types.find(name);
    if (type != var_types.end()) {
// #ifdef DEBUG
//       std::cout << "lookup local type: " << name << " line: " << line
//                 << " type: " << var_type_to_string(type->second) << " found"
//                 << std::endl;
// #endif
      return type->second;
    }
  }

  // 2. 在父作用域查找
  if (parent) {
    return parent->lookup_var_type(name);
  }

  // 3. 在全局作用域查找
  return ctx.get_var_type(name, line);
}

/* global关键字添加的全局变量标志 */
void ASTNode::add_global_var(const std::string &name) {
  // 添加到最近作用域
  if (is_scope) {
    global_vars.insert(name);
    return;
  }

  if (parent) {
    parent->add_global_var(name);
  } else {
    // 根节点并且非作用域不允许添加global关键字
    throw std::runtime_error("not found scope: " + name +
                             " line: " + std::to_string(line) + " file: " + __FILE__ + " line: " + std::to_string(__LINE__));
  }
}

bool ASTNode::is_global_var(const std::string &name) const {
  // 只在当前作用域查找
  if (is_scope) {
    return global_vars.find(name) != global_vars.end();
  }
  if (parent) {
    return parent->is_global_var(name);
  }
  return false;
}

int ASTNode::get_scope_depth() const {
  int depth = 0;
  const ASTNode *curr = parent;
  while (curr) {
    if (curr->is_scope)
      depth++;
    curr = curr->parent;
  }
  return depth;
}

std::string ASTNode::get_scope_path() const {
  std::string path;
  const ASTNode *curr = this;
  while (curr) {
    if (curr->is_scope) {
      path = "." + std::to_string(curr->line) + path;
    }
    curr = curr->parent;
  }
  return path;
}

void ASTNode::add_llvm_symbol(const std::string &name, llvm::Value *value) {
  // 如果当前作用域存在该变量，则添加到符号表
  if (is_scope) {
    auto type = var_types.find(name);
    if (type != var_types.end()) {
    //   std::cout << "add llvm symbol: " << name << " line: " << line
    //             << std::endl;
      symbols[name] = value;
      return;
    }
  }
  if (parent) {
    parent->add_llvm_symbol(name, value);
  }
  ctx.add_llvm_symbol(name, value);
}

llvm::Value *ASTNode::lookup_llvm_symbol(const std::string &name) const {
  // 1. 在当前作用域查找
  if (is_scope) {
    auto it = symbols.find(name);
    if (it != symbols.end()) {
      return it->second;
    }
  }

  // 2. 在父作用域查找
  if (parent) {
    return parent->lookup_llvm_symbol(name);
  }

  // 3. 在全局作用域查找
  return ctx.lookup_llvm_symbol(name);
}

// int ASTNode::erase_llvm_symbol(const std::string &name) {
//   if (is_scope) {
//     auto it = symbols.find(name);
//     if (it != symbols.end()) {
//       symbols.erase(it);
//       return 0;
//     } else {
// #ifdef DEBUG
//       std::cout << "非法移除符号: " << name << " line: " << line << std::endl;
// #endif
//       return -1;
//     }
//   }
//   if (parent) {
//     return parent->erase_llvm_symbol(name);
//   }
//   ctx.erase_llvm_symbol(name);
//   return -1;
// }
