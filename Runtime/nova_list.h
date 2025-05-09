#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "nova_memory_manager.h"
#include "nova_string.h"

// List structure (memory layout)
// The memory layout of a list is:
// - size_t count (number of elements)
// - Followed by elements (each 8 bytes for alignment)

// Constants for list printing
#define NOVA_LIST_MAX_DISPLAY_ITEMS 10
#define NOVA_LIST_TRUNCATION "..."

// Element types (matching VarType enum in compiler)
typedef enum {
    NOVA_TYPE_NONE = 0,
    NOVA_TYPE_VOID = 1,
    NOVA_TYPE_INT = 2,
    NOVA_TYPE_FLOAT = 3,
    NOVA_TYPE_BOOL = 4,
    NOVA_TYPE_STRING = 5,
    NOVA_TYPE_STRUCT = 6,
    NOVA_TYPE_DICT = 7,
    NOVA_TYPE_LIST = 8,
    NOVA_TYPE_FUNCTION = 9,
    NOVA_TYPE_CLASS = 10,
    NOVA_TYPE_INSTANCE = 11
} nova_element_type;

// Convert list to string for printing
// Truncates long lists with ellipsis
char* nova_list_to_string(nova_memory_block* list_block, nova_element_type elem_type); 