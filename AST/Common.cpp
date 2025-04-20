#include "Common.h"

std::string var_type_to_string(VarType type) {
  switch (type) {
  case VarType::INT:
    return "int";
  case VarType::FLOAT:
    return "float";
  case VarType::BOOL:
    return "bool";
  case VarType::STRING:
    return "string";
  case VarType::STRUCT:
    return "struct";
  case VarType::DICT:
    return "dict";
  case VarType::LIST:
    return "list";
  case VarType::VOID:
    return "void";
  case VarType::NONE:
    return "none";
  }
  return "unknown";
}