#include "nova_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Maximum buffer size for string representation
#define MAX_BUFFER_SIZE 4096

// Helper function to append a string to a buffer with bounds checking
static void append_to_buffer(char* buffer, size_t* pos, size_t max_size, const char* str) {
    size_t len = strlen(str);
    if (*pos + len >= max_size) {
        len = max_size - *pos - 1;  // Leave space for null terminator
    }
    if (len > 0) {
        memcpy(buffer + *pos, str, len);
        *pos += len;
    }
    buffer[*pos] = '\0';  // Ensure null termination
}

// Convert list to string for printing
char* nova_list_to_string(nova_memory_block* list_block, nova_element_type elem_type) {
    if (!list_block) {
        return strdup("null");
    }

    // Allocate a buffer for the string representation
    char* buffer = (char*)malloc(MAX_BUFFER_SIZE);
    if (!buffer) {
        return strdup("[内存分配失败]");
    }
    
    size_t pos = 0;
    buffer[0] = '\0';
    
    // Get data pointer
    void* data = nova_memory_get_data(list_block);
    if (!data) {
        free(buffer);
        return strdup("[无效列表]");
    }
    
    // Get element count
    size_t count = *((size_t*)data);
    
    // Start with opening bracket
    append_to_buffer(buffer, &pos, MAX_BUFFER_SIZE, "[");
    
    // Get pointer to first element (after count)
    uint8_t* elem_ptr = ((uint8_t*)data) + sizeof(size_t);
    
    // Display elements up to the maximum or the actual count
    size_t display_count = count;
    bool truncated = false;
    
    if (count > NOVA_LIST_MAX_DISPLAY_ITEMS) {
        display_count = NOVA_LIST_MAX_DISPLAY_ITEMS;
        truncated = true;
    }
    
    // Element string buffer
    char elem_str[128];
    
    // Process each element based on its known type
    for (size_t i = 0; i < display_count; i++) {
        // Format the element based on its known type
        switch (elem_type) {
            case NOVA_TYPE_INT: {
                int64_t* val_ptr = (int64_t*)(elem_ptr + i * 8);
                snprintf(elem_str, sizeof(elem_str), "%ld", *val_ptr);
                break;
            }
            case NOVA_TYPE_FLOAT: {
                double* val_ptr = (double*)(elem_ptr + i * 8);
                snprintf(elem_str, sizeof(elem_str), "%g", *val_ptr);
                break;
            }
            case NOVA_TYPE_BOOL: {
                bool* val_ptr = (bool*)(elem_ptr + i * 8);
                strcpy(elem_str, *val_ptr ? "True" : "False");
                break;
            }
            case NOVA_TYPE_STRING: {
                // String elements are pointers to nova_memory_block
                nova_memory_block** val_ptr = (nova_memory_block**)(elem_ptr + i * 8);
                nova_memory_block* str_block = *val_ptr;
                
                if (str_block) {
                    char* str = string_to_system(str_block);
                    if (str) {
                        snprintf(elem_str, sizeof(elem_str), "\"%s\"", str);
                        free(str); // Free the converted string
                    } else {
                        strcpy(elem_str, "\"\"");
                    }
                } else {
                    strcpy(elem_str, "null");
                }
                break;
            }
            case NOVA_TYPE_LIST: {
                // Nested lists - just print a placeholder
                strcpy(elem_str, "[...]");
                break;
            }
            case NOVA_TYPE_DICT: {
                // Dictionary elements - just print a placeholder
                strcpy(elem_str, "{...}");
                break;
            }
            default: {
                // For other types like structs, functions, etc.
                strcpy(elem_str, "<对象>");
                break;
            }
        }
        
        // Add element string to buffer
        append_to_buffer(buffer, &pos, MAX_BUFFER_SIZE, elem_str);
        
        // Add separator if not the last element
        if (i < display_count - 1 || truncated) {
            append_to_buffer(buffer, &pos, MAX_BUFFER_SIZE, ", ");
        }
    }
    
    // Add truncation indicator if needed
    if (truncated) {
        append_to_buffer(buffer, &pos, MAX_BUFFER_SIZE, NOVA_LIST_TRUNCATION);
    }
    
    // Close with closing bracket
    append_to_buffer(buffer, &pos, MAX_BUFFER_SIZE, "]");
    
    return buffer;
} 