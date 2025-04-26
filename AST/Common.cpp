#include <iostream>
#include "Common.h"
#include <execinfo.h>

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
  case VarType::FUNCTION:
    return "function";
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
  case VarType::FUNCTION:  // 函数指针也是指针类型，使用与其他指针类型相同的对齐值
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

void print_backtrace() {
  void *array[10];
  size_t size;

  // 获取调用栈信息
  size = backtrace(array, 10);

  std::string program_name = std::string(program_invocation_name);
  // 打印调用栈信息
  char **strings = backtrace_symbols(array, size);
  if (strings != nullptr) {
    bool skip = false;
    for (size_t i = 0; i < size; ++i) {
      if (strncmp(strings[i], program_name.c_str(), program_name.length()) != 0)
        continue;

      if (!skip) {
        skip = true;
        continue;
      }

      // 提取地址
      char *address_start = std::strchr(strings[i], '(');
      if (address_start != nullptr) {
        address_start++;
        char *address_end = std::strchr(address_start, ')');
        if (address_end != nullptr) {
          *address_end = '\0';
          std::string address = address_start;

          // 调用 addr2line 工具
          std::string command = "addr2line -e " + program_name + " " + address;
          std::system(command.c_str());
        }
      }
    }
    free(strings);
  }
}