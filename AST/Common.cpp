#include <iostream>
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
// 获取类型对齐值
uint32_t get_type_align(VarType type) {
  switch (type) {
  case VarType::INT:
  case VarType::FLOAT:
    return 8;
  case VarType::BOOL:
    return 1;
  case VarType::STRING:
  case VarType::STRUCT:
  case VarType::DICT:
  case VarType::LIST:
    return 8; // Pointer alignment
  default:
    std::cerr << "Unknown type: " << var_type_to_string(type) << std::endl;
    return 0;
  }
}

// 获取两个类型中较大的对齐值
uint32_t get_max_align(VarType type1, VarType type2) {
  return std::max(get_type_align(type1), get_type_align(type2));
}