#include "Common.h"

std::string var_type_to_string(VarType type) {
  switch (type) {
  case VarType::INT:
    return "int";
  case VarType::FLOAT:
    return "float";
  case VarType::BOOL:
    return "bool";
  case VarType::VOID:
    return "void";
  case VarType::NONE:
    return "none";
  }
  return "unknown";
}