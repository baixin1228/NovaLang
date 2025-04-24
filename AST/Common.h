#pragma once
#include <string>
#include <llvm/IR/Value.h>

enum class VarType {
    VOID,
    INT,
    FLOAT,
    BOOL,
    STRING,
    STRUCT,
    DICT,
    LIST,
    NONE
};

struct VarInfo {
  int line;
  VarType type;
  llvm::Value *llvm_obj;
};

struct FuncInfo {
  int line;
  int ast_index;
  bool need_conjecture;
  VarType return_type;
  llvm::Function *llvm_obj;
  std::vector<VarType> param_types;
};

std::string var_type_to_string(VarType type);
uint32_t get_type_align(VarType type);
uint32_t get_max_align(VarType type1, VarType type2);
