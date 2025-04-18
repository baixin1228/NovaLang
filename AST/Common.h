#pragma once
#include <string>

enum class VarType {
  INT,
  FLOAT,
  BOOL,
  VOID,
  NONE,
};

std::string var_type_to_string(VarType type);
