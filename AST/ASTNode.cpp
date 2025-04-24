#include "ASTNode.h"
#include "Context.h"

ASTNode::ASTNode(Context &ctx, int ln, bool is_scope)
    : line(ln), parent(nullptr), ctx(ctx), is_scope(is_scope) {}

void ASTNode::set_parent(ASTNode *p) { parent = p; }

/* add variable to current scope or global scope */
int ASTNode::add_var_info(const std::string &name, bool is_params) {
  // if it is global variable, set it in global scope
  if (is_global_var(name)) {
    if (is_params) {
      throw std::runtime_error("global already exists: " + name +
                               " source_line: " + std::to_string(line) + " file: " + __FILE__ + " line: " + std::to_string(__LINE__));
    }
    return ctx.add_var_info(name, line);
  }

  if (is_scope) {
    if (var_infos.find(name) != var_infos.end()) {
      return -1;
    }
    var_infos[name] = {.line = line};
    return 0;
  }

  if (parent) {
    return parent->add_var_info(name, is_params);
  }

  return ctx.add_var_info(name, line);
}

VarInfo& ASTNode::lookup_var_info(const std::string &name) {
  if (name.empty()) {
    throw std::runtime_error("variable name is empty: " + name +
                             " line: " + std::to_string(line) + " file: " + __FILE__ + " line: " + std::to_string(__LINE__));
  }

  if (is_scope) {
    auto info = var_infos.find(name);
    if (info != var_infos.end()) {
      return info->second;
    }
  }
  if (parent) {
    return parent->lookup_var_info(name);
  }
  return ctx.lookup_var_info(name, line);
}

int ASTNode::_add_func_info(const std::string &name) {
  if (is_scope) {
    func_infos[name] = {};
    return 0;
  }
  if (parent) {
    return parent->_add_func_info(name);
  }

  return ctx.add_func_info(name);
}

int ASTNode::add_func_info(const std::string &name) {
  if (parent) {
    return parent->_add_func_info(name);
  }

  return ctx.add_func_info(name);
}

FuncInfo& ASTNode::lookup_func_info(const std::string &name) {
  if (is_scope) {
    auto info = func_infos.find(name);
    if (info != func_infos.end()) {
      return info->second;
    }
  }
  if (parent) {
    return parent->lookup_func_info(name);
  }
  return ctx.get_func_info(name);
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

void ASTNode::add_var_llvm_obj(const std::string &name, llvm::Value *value) {
  lookup_var_info(name).llvm_obj = value;
}

llvm::Value *ASTNode::lookup_var_llvm_obj(const std::string &name) {
  return lookup_var_info(name).llvm_obj;
}