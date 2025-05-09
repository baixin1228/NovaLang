#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "nova_memory_manager.h"
#include "nova_string.h"

// Dictionary entry structure to hold key-value pairs
typedef struct nova_dict_entry {
    // Keys can be string or numbers (int/float)
    union {
        nova_memory_block* str_key; // String is managed by nova_memory_block
        int64_t int_key;
        double float_key;
    };
    
    // Values can be int, float, bool, or pointer
    union {
        int64_t int_val;
        double float_val;
        bool bool_val;
        void* ptr_val;
    };
    
    // Type of key (0 = string, 1 = int, 2 = float)
    uint8_t key_type;
    
    // Type of value (0 = int, 1 = float, 2 = bool, 3 = pointer)
    uint8_t val_type;
    
    // Flag to indicate if this entry is used
    bool is_occupied;
    
    // Next entry in case of collision
    struct nova_dict_entry* next;
} nova_dict_entry;

// Dictionary structure
typedef struct {
    // Number of entries in the dictionary
    uint32_t size;
    
    // Type of key (matches VarType enum in compiler)
    uint8_t key_type;
    
    // Type of value (matches VarType enum in compiler)
    uint8_t value_type;
    
    // Hash table size (default 1024)
    uint32_t bucket_count;
    
    // Buckets as flexible array
    nova_dict_entry buckets[];
} nova_dict;

// Create a new dictionary
nova_memory_block* nova_dict_new(uint8_t key_type, uint8_t value_type);

// Free a dictionary
void nova_dict_free(nova_memory_block* dict_block);

// Set functions for different key and value types
void nova_dict_set_str_int(nova_memory_block* dict_block, nova_memory_block* key, int64_t value);
void nova_dict_set_str_float(nova_memory_block* dict_block, nova_memory_block* key, double value);
void nova_dict_set_str_bool(nova_memory_block* dict_block, nova_memory_block* key, bool value);
void nova_dict_set_str_ptr(nova_memory_block* dict_block, nova_memory_block* key, void* value);

void nova_dict_set_int_int(nova_memory_block* dict_block, int64_t key, int64_t value);
void nova_dict_set_int_float(nova_memory_block* dict_block, int64_t key, double value);
void nova_dict_set_int_bool(nova_memory_block* dict_block, int64_t key, bool value);
void nova_dict_set_int_ptr(nova_memory_block* dict_block, int64_t key, void* value);

// Get functions for different key and value types
bool nova_dict_get_str_int(nova_memory_block* dict_block, nova_memory_block* key, int64_t* value);
bool nova_dict_get_str_float(nova_memory_block* dict_block, nova_memory_block* key, double* value);
bool nova_dict_get_str_bool(nova_memory_block* dict_block, nova_memory_block* key, bool* value);
bool nova_dict_get_str_ptr(nova_memory_block* dict_block, nova_memory_block* key, void** value);

bool nova_dict_get_int_int(nova_memory_block* dict_block, int64_t key, int64_t* value);
bool nova_dict_get_int_float(nova_memory_block* dict_block, int64_t key, double* value);
bool nova_dict_get_int_bool(nova_memory_block* dict_block, int64_t key, bool* value);
bool nova_dict_get_int_ptr(nova_memory_block* dict_block, int64_t key, void** value);

// Check if a key exists in the dictionary
bool nova_dict_contains_str(nova_memory_block* dict_block, nova_memory_block* key);
bool nova_dict_contains_int(nova_memory_block* dict_block, int64_t key);

// Get the number of entries in the dictionary
uint32_t nova_dict_size(nova_memory_block* dict_block);

// Remove an entry from the dictionary
bool nova_dict_remove_str_key(nova_memory_block* dict_block, nova_memory_block* key);
bool nova_dict_remove_int_key(nova_memory_block* dict_block, int64_t key);

// Convert dictionary to string for printing
char* nova_dict_to_string(nova_memory_block* dict_block);
