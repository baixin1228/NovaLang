#pragma once
#include <string>

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

std::string var_type_to_string(VarType type);
