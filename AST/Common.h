#pragma once
#include <string>
#include <vector>
#include <llvm/IR/Value.h>

// Forward declarations for LLVM types
namespace llvm {
  class Function;
  class StructType;
}

enum class VarType {
  NONE,
  VOID,
  INT,
  FLOAT,
  BOOL,
  STRING,
  STRUCT,
  DICT,
  LIST,
  FUNCTION,
  CLASS,
  INSTANCE,
};

std::string var_type_to_string(VarType type);
uint32_t get_type_align(VarType type);
uint32_t get_max_align(VarType type1, VarType type2);
void print_backtrace();