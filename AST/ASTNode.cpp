#include "ASTNode.h"
#include "Context.h"
#include "Function.h"
#include "StructLiteral.h"

ASTNode::ASTNode(Context &ctx, int ln, bool is_scope)
    : line(ln), parent(nullptr), ctx(ctx), is_scope(is_scope) {}

void ASTNode::set_parent(ASTNode *p) { parent = p; }

/* add variable to current scope or global scope */
int ASTNode::add_var(const std::string &name,
                          std::shared_ptr<ASTNode> node, bool is_params) {
  // if it is global variable, set it in global scope
  if (is_global_var(name)) {
    if (is_params) {
#ifdef DEBUG
      std::cout << "-- global already exists: " << name << " " << line << std::endl;
#endif
      return -1;
    }
    return ctx.add_global_var(name, node);
  }

  if (is_scope) {
    if (vars.find(name) != vars.end()) {
      return -1;
    }
    vars[name] = node;
// #ifdef DEBUG
//   std::cout << "-- add local var: " << name << " " << line << std::endl;
// #endif
    return 0;
  }

  if (parent) {
    return parent->add_var(name, node, is_params);
  }

  return ctx.add_global_var(name, node);
}

std::shared_ptr<ASTNode> ASTNode::lookup_var(const std::string &name, int p_line) {
  if (name.empty()) {
    throw std::runtime_error("variable name is empty: " + name +
                             " line: " + std::to_string(line) + " file: " + __FILE__ + " line: " + std::to_string(__LINE__));
  }
  // std::cout << "-- lookup var: " << name << " " << p_line << std::endl;
  if (is_scope) {
    auto info = vars.find(name);
    if (info != vars.end()) {
      // std::cout << "-- lookup local var: " << name << " " << p_line << " "
      //           << info->second  << std::endl;
      return info->second;
    }
  }
  if (parent) {
    return parent->lookup_var(name, p_line);
  }
  return ctx.lookup_global_var(name, p_line);
}

int ASTNode::_add_func(const std::string &name, std::shared_ptr<ASTNode> node) {
  if (is_scope) {
    func_infos[name] = node;
    return 0;
  }
  if (parent) {
    return parent->_add_func(name, node);
  }

  return ctx.add_global_func(name, node);
}

int ASTNode::add_func(const std::string &name, std::shared_ptr<ASTNode> node) {
  if (parent) {
    return parent->_add_func(name, node);
  }

  return ctx.add_global_func(name, node);
}

std::shared_ptr<ASTNode> ASTNode::lookup_func(const std::string &name) {
  if (is_scope) {
    auto info = func_infos.find(name);
    if (info != func_infos.end()) {
      return info->second;
    }
  }
  if (parent) {
    return parent->lookup_func(name);
  }
  return ctx.lookup_global_func(name);
}

void ASTNode::add_global_var(const std::string &name) {
  // add to current scope
  if (is_scope) {
    global_vars.insert(name);
    return;
  }

  if (parent) {
    parent->add_global_var(name);
  } else {
    // root node and not scope, not allowed to add global keyword
    throw std::runtime_error("not found scope: " + name +
                             " line: " + std::to_string(line) + " file: " + __FILE__ + " line: " + std::to_string(__LINE__));
  }
}

bool ASTNode::is_global_var(const std::string &name) const {
  // find in current scope
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