#include "nova_dict.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define NOVA_DICT_BUCKET_COUNT 1024
#define NOVA_DICT_ARRAY_SIZE 16  // Size of array in each bucket

// Hash function for string keys
static uint32_t hash_string(nova_memory_block* str) {
    uint32_t hash = 5381;
    unicode_string* ustr = (unicode_string*)nova_memory_get_data(str);
    
    for (int32_t i = 0; i < ustr->length; i++) {
        hash = ((hash << 5) + hash) + ustr->data[i]; // hash * 33 + c
    }
    
    return hash % NOVA_DICT_BUCKET_COUNT;
}

// Hash function for integer keys
static uint32_t hash_int(int64_t key) {
    return (uint32_t)((key ^ (key >> 32)) % NOVA_DICT_BUCKET_COUNT);
}

// Hash function for float keys
static uint32_t hash_float(double key) {
    // Convert double to int64_t bits and then hash
    int64_t bits;
    memcpy(&bits, &key, sizeof(double));
    return hash_int(bits);
}

// Create a new dictionary
nova_memory_block* nova_dict_new(uint8_t key_type, uint8_t value_type) {
    // Allocate memory for the dictionary and its buckets
    size_t size = sizeof(nova_dict) + (NOVA_DICT_BUCKET_COUNT * NOVA_DICT_ARRAY_SIZE * sizeof(nova_dict_entry));
    nova_memory_block* dict_block = nova_memory_alloc(size);
    if (!dict_block) {
        return NULL;
    }
    
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    dict->size = 0;
    dict->key_type = key_type;
    dict->value_type = value_type;
    dict->bucket_count = NOVA_DICT_BUCKET_COUNT;
    
    // Initialize all entries
    for (uint32_t i = 0; i < NOVA_DICT_BUCKET_COUNT * NOVA_DICT_ARRAY_SIZE; i++) {
        dict->buckets[i].is_occupied = false;
        dict->buckets[i].next = NULL;
    }
    
    return dict_block;
}

// Free a dictionary
void nova_dict_free(nova_memory_block* dict_block) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    
    // Free any dynamically allocated entries (for collision chains)
    for (uint32_t i = 0; i < dict->bucket_count * NOVA_DICT_ARRAY_SIZE; i++) {
        nova_dict_entry* entry = dict->buckets[i].next;
        while (entry) {
            nova_dict_entry* next = entry->next;
            
            // Release string key if needed
            if (entry->key_type == 0 && entry->is_occupied) { // String key
                nova_memory_release(entry->str_key);
            }
            
            free(entry);
            entry = next;
        }
        
        // Release string key in main array if needed
        if (dict->buckets[i].key_type == 0 && dict->buckets[i].is_occupied) {
            nova_memory_release(dict->buckets[i].str_key);
        }
    }
    
    nova_memory_release(dict_block);
}

// Helper function to find entry with string key
static nova_dict_entry* find_entry_str(nova_dict* dict, nova_memory_block* key, uint32_t* bucket_idx) {
    uint32_t hash = hash_string(key);
    *bucket_idx = hash;
    
    // Calculate base index in the flattened array
    uint32_t base_idx = hash * NOVA_DICT_ARRAY_SIZE;
    unicode_string* key_str = (unicode_string*)nova_memory_get_data(key);
    
    // Check entries in the array for this bucket
    for (uint32_t i = 0; i < NOVA_DICT_ARRAY_SIZE; i++) {
        nova_dict_entry* entry = &dict->buckets[base_idx + i];
        
        if (entry->is_occupied && entry->key_type == 0) { // String key
            unicode_string* entry_str = (unicode_string*)nova_memory_get_data(entry->str_key);
            
            // Check if strings match
            if (entry_str->length == key_str->length) {
                if (memcmp(entry_str->data, key_str->data, key_str->length * sizeof(UChar)) == 0) {
                    return entry;
                }
            }
        }
    }
    
    // Check overflow chain
    nova_dict_entry* entry = dict->buckets[base_idx].next;
    while (entry) {
        if (entry->is_occupied && entry->key_type == 0) { // String key
            unicode_string* entry_str = (unicode_string*)nova_memory_get_data(entry->str_key);
            
            // Check if strings match
            if (entry_str->length == key_str->length) {
                if (memcmp(entry_str->data, key_str->data, key_str->length * sizeof(UChar)) == 0) {
                    return entry;
                }
            }
        }
        entry = entry->next;
    }
    
    return NULL;
}

// Helper function to find entry with integer key
static nova_dict_entry* find_entry_int(nova_dict* dict, int64_t key, uint32_t* bucket_idx) {
    uint32_t hash = hash_int(key);
    *bucket_idx = hash;
    
    // Calculate base index in the flattened array
    uint32_t base_idx = hash * NOVA_DICT_ARRAY_SIZE;
    
    // Check entries in the array for this bucket
    for (uint32_t i = 0; i < NOVA_DICT_ARRAY_SIZE; i++) {
        nova_dict_entry* entry = &dict->buckets[base_idx + i];
        
        if (entry->is_occupied && entry->key_type == 1 && entry->int_key == key) { // Integer key match
            return entry;
        }
    }
    
    // Check overflow chain
    nova_dict_entry* entry = dict->buckets[base_idx].next;
    while (entry) {
        if (entry->is_occupied && entry->key_type == 1 && entry->int_key == key) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

// Helper function to create a new entry in case of collision
static nova_dict_entry* create_overflow_entry(nova_dict* dict, uint32_t bucket_idx) {
    nova_dict_entry* new_entry = (nova_dict_entry*)malloc(sizeof(nova_dict_entry));
    if (!new_entry) {
        return NULL;
    }
    
    new_entry->is_occupied = false;
    new_entry->next = NULL;
    
    // Add to the chain
    uint32_t base_idx = bucket_idx * NOVA_DICT_ARRAY_SIZE;
    nova_dict_entry* head = &dict->buckets[base_idx];
    
    if (head->next == NULL) {
        head->next = new_entry;
    } else {
        nova_dict_entry* current = head->next;
        while (current->next) {
            current = current->next;
        }
        current->next = new_entry;
    }
    
    return new_entry;
}

// Helper function to find or create an entry for string key
static nova_dict_entry* find_or_create_entry_str(nova_dict* dict, nova_memory_block* key, bool* created) {
    uint32_t bucket_idx;
    nova_dict_entry* entry = find_entry_str(dict, key, &bucket_idx);
    
    if (entry) {
        *created = false;
        return entry;
    }
    
    *created = true;
    
    // Try to find an empty slot in the array
    uint32_t base_idx = bucket_idx * NOVA_DICT_ARRAY_SIZE;
    for (uint32_t i = 0; i < NOVA_DICT_ARRAY_SIZE; i++) {
        if (!dict->buckets[base_idx + i].is_occupied) {
            return &dict->buckets[base_idx + i];
        }
    }
    
    // All slots full, create overflow entry
    return create_overflow_entry(dict, bucket_idx);
}

// Helper function to find or create an entry for integer key
static nova_dict_entry* find_or_create_entry_int(nova_dict* dict, int64_t key, bool* created) {
    uint32_t bucket_idx;
    nova_dict_entry* entry = find_entry_int(dict, key, &bucket_idx);
    
    if (entry) {
        *created = false;
        return entry;
    }
    
    *created = true;
    
    // Try to find an empty slot in the array
    uint32_t base_idx = bucket_idx * NOVA_DICT_ARRAY_SIZE;
    for (uint32_t i = 0; i < NOVA_DICT_ARRAY_SIZE; i++) {
        if (!dict->buckets[base_idx + i].is_occupied) {
            return &dict->buckets[base_idx + i];
        }
    }
    
    // All slots full, create overflow entry
    return create_overflow_entry(dict, bucket_idx);
}

// Set functions for string keys
void nova_dict_set_str_int(nova_memory_block* dict_block, nova_memory_block* key, int64_t value) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    bool created;
    
    nova_dict_entry* entry = find_or_create_entry_str(dict, key, &created);
    if (!entry) {
        return; // Error handling
    }
    
    if (created) {
        entry->key_type = 0; // String key
        entry->str_key = key;
        nova_memory_retain(key);
        dict->size++;
    } else if (entry->key_type == 0 && entry->val_type != 0) {
        // Value type changed, release old string key if needed
    }
    
    entry->val_type = 0; // Integer value
    entry->int_val = value;
    entry->is_occupied = true;
}

void nova_dict_set_str_float(nova_memory_block* dict_block, nova_memory_block* key, double value) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    bool created;
    
    nova_dict_entry* entry = find_or_create_entry_str(dict, key, &created);
    if (!entry) {
        return; // Error handling
    }
    
    if (created) {
        entry->key_type = 0; // String key
        entry->str_key = key;
        nova_memory_retain(key);
        dict->size++;
    }
    
    entry->val_type = 1; // Float value
    entry->float_val = value;
    entry->is_occupied = true;
}

void nova_dict_set_str_bool(nova_memory_block* dict_block, nova_memory_block* key, bool value) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    bool created;
    
    nova_dict_entry* entry = find_or_create_entry_str(dict, key, &created);
    if (!entry) {
        return; // Error handling
    }
    
    if (created) {
        entry->key_type = 0; // String key
        entry->str_key = key;
        nova_memory_retain(key);
        dict->size++;
    }
    
    entry->val_type = 2; // Boolean value
    entry->bool_val = value;
    entry->is_occupied = true;
}

void nova_dict_set_str_ptr(nova_memory_block* dict_block, nova_memory_block* key, void* value) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    bool created;
    
    nova_dict_entry* entry = find_or_create_entry_str(dict, key, &created);
    if (!entry) {
        return; // Error handling
    }
    
    if (created) {
        entry->key_type = 0; // String key
        entry->str_key = key;
        nova_memory_retain(key);
        dict->size++;
    }
    
    entry->val_type = 3; // Pointer value
    entry->ptr_val = value;
    entry->is_occupied = true;
}

// Set functions for integer keys
void nova_dict_set_int_int(nova_memory_block* dict_block, int64_t key, int64_t value) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    bool created;
    
    nova_dict_entry* entry = find_or_create_entry_int(dict, key, &created);
    if (!entry) {
        return; // Error handling
    }
    
    if (created) {
        entry->key_type = 1; // Integer key
        entry->int_key = key;
        dict->size++;
    }
    
    entry->val_type = 0; // Integer value
    entry->int_val = value;
    entry->is_occupied = true;
}

void nova_dict_set_int_float(nova_memory_block* dict_block, int64_t key, double value) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    bool created;
    
    nova_dict_entry* entry = find_or_create_entry_int(dict, key, &created);
    if (!entry) {
        return; // Error handling
    }
    
    if (created) {
        entry->key_type = 1; // Integer key
        entry->int_key = key;
        dict->size++;
    }
    
    entry->val_type = 1; // Float value
    entry->float_val = value;
    entry->is_occupied = true;
}

void nova_dict_set_int_bool(nova_memory_block* dict_block, int64_t key, bool value) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    bool created;
    
    nova_dict_entry* entry = find_or_create_entry_int(dict, key, &created);
    if (!entry) {
        return; // Error handling
    }
    
    if (created) {
        entry->key_type = 1; // Integer key
        entry->int_key = key;
        dict->size++;
    }
    
    entry->val_type = 2; // Boolean value
    entry->bool_val = value;
    entry->is_occupied = true;
}

void nova_dict_set_int_ptr(nova_memory_block* dict_block, int64_t key, void* value) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    bool created;
    
    nova_dict_entry* entry = find_or_create_entry_int(dict, key, &created);
    if (!entry) {
        return; // Error handling
    }
    
    if (created) {
        entry->key_type = 1; // Integer key
        entry->int_key = key;
        dict->size++;
    }
    
    entry->val_type = 3; // Pointer value
    entry->ptr_val = value;
    entry->is_occupied = true;
}

// Get functions for string keys
bool nova_dict_get_str_int(nova_memory_block* dict_block, nova_memory_block* key, int64_t* value) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    uint32_t bucket_idx;
    
    nova_dict_entry* entry = find_entry_str(dict, key, &bucket_idx);
    if (!entry || !entry->is_occupied || entry->val_type != 0) {
        return false;
    }
    
    *value = entry->int_val;
    return true;
}

bool nova_dict_get_str_float(nova_memory_block* dict_block, nova_memory_block* key, double* value) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    uint32_t bucket_idx;
    
    nova_dict_entry* entry = find_entry_str(dict, key, &bucket_idx);
    if (!entry || !entry->is_occupied || entry->val_type != 1) {
        return false;
    }
    
    *value = entry->float_val;
    return true;
}

bool nova_dict_get_str_bool(nova_memory_block* dict_block, nova_memory_block* key, bool* value) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    uint32_t bucket_idx;
    
    nova_dict_entry* entry = find_entry_str(dict, key, &bucket_idx);
    if (!entry || !entry->is_occupied || entry->val_type != 2) {
        return false;
    }
    
    *value = entry->bool_val;
    return true;
}

bool nova_dict_get_str_ptr(nova_memory_block* dict_block, nova_memory_block* key, void** value) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    uint32_t bucket_idx;
    
    nova_dict_entry* entry = find_entry_str(dict, key, &bucket_idx);
    if (!entry || !entry->is_occupied || entry->val_type != 3) {
        return false;
    }
    
    *value = entry->ptr_val;
    return true;
}

// Get functions for integer keys
bool nova_dict_get_int_int(nova_memory_block* dict_block, int64_t key, int64_t* value) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    uint32_t bucket_idx;
    
    nova_dict_entry* entry = find_entry_int(dict, key, &bucket_idx);
    if (!entry || !entry->is_occupied || entry->val_type != 0) {
        return false;
    }
    
    *value = entry->int_val;
    return true;
}

bool nova_dict_get_int_float(nova_memory_block* dict_block, int64_t key, double* value) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    uint32_t bucket_idx;
    
    nova_dict_entry* entry = find_entry_int(dict, key, &bucket_idx);
    if (!entry || !entry->is_occupied || entry->val_type != 1) {
        return false;
    }
    
    *value = entry->float_val;
    return true;
}

bool nova_dict_get_int_bool(nova_memory_block* dict_block, int64_t key, bool* value) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    uint32_t bucket_idx;
    
    nova_dict_entry* entry = find_entry_int(dict, key, &bucket_idx);
    if (!entry || !entry->is_occupied || entry->val_type != 2) {
        return false;
    }
    
    *value = entry->bool_val;
    return true;
}

bool nova_dict_get_int_ptr(nova_memory_block* dict_block, int64_t key, void** value) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    uint32_t bucket_idx;
    
    nova_dict_entry* entry = find_entry_int(dict, key, &bucket_idx);
    if (!entry || !entry->is_occupied || entry->val_type != 3) {
        return false;
    }
    
    *value = entry->ptr_val;
    return true;
}

// Check if a key exists in the dictionary
bool nova_dict_contains_str(nova_memory_block* dict_block, nova_memory_block* key) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    uint32_t bucket_idx;
    
    nova_dict_entry* entry = find_entry_str(dict, key, &bucket_idx);
    return (entry != NULL && entry->is_occupied);
}

bool nova_dict_contains_int(nova_memory_block* dict_block, int64_t key) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    uint32_t bucket_idx;
    
    nova_dict_entry* entry = find_entry_int(dict, key, &bucket_idx);
    return (entry != NULL && entry->is_occupied);
}

// Get the number of entries in the dictionary
uint32_t nova_dict_size(nova_memory_block* dict_block) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    return dict->size;
}

// Remove an entry from the dictionary
bool nova_dict_remove_str_key(nova_memory_block* dict_block, nova_memory_block* key) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    uint32_t bucket_idx;
    
    nova_dict_entry* entry = find_entry_str(dict, key, &bucket_idx);
    if (!entry || !entry->is_occupied) {
        return false;
    }
    
    // Release string key
    if (entry->key_type == 0) {
        nova_memory_release(entry->str_key);
    }
    
    entry->is_occupied = false;
    dict->size--;
    
    return true;
}

bool nova_dict_remove_int_key(nova_memory_block* dict_block, int64_t key) {
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    uint32_t bucket_idx;
    
    nova_dict_entry* entry = find_entry_int(dict, key, &bucket_idx);
    if (!entry || !entry->is_occupied) {
        return false;
    }
    
    entry->is_occupied = false;
    dict->size--;
    
    return true;
}

// Convert dictionary to string for printing
char* nova_dict_to_string(nova_memory_block* dict_block) {
    if (!dict_block) {
        return strdup("{}");
    }
    
    nova_dict* dict = (nova_dict*)nova_memory_get_data(dict_block);
    
    // 初始分配大小，根据实际情况可能需要扩展
    size_t buf_size = 1024;
    char* buffer = (char*)malloc(buf_size);
    if (!buffer) {
        return strdup("{内存不足}");
    }
    
    // 初始化缓冲区
    buffer[0] = '{';
    buffer[1] = '\0';
    size_t pos = 1;
    bool first_item = true;
    
    // 遍历所有桶和项
    for (uint32_t i = 0; i < dict->bucket_count * NOVA_DICT_ARRAY_SIZE; i++) {
        nova_dict_entry* entry = &dict->buckets[i];
        if (entry->is_occupied) {
            // 如果不是第一项，添加逗号
            if (!first_item) {
                strcpy(buffer + pos, ", ");
                pos += 2;
            } else {
                first_item = false;
            }
            
            // 确保缓冲区有足够空间
            if (pos + 100 >= buf_size) {
                buf_size *= 2;
                char* new_buffer = (char*)realloc(buffer, buf_size);
                if (!new_buffer) {
                    free(buffer);
                    return strdup("{内存不足}");
                }
                buffer = new_buffer;
            }
            
            // 根据键类型格式化键
            if (entry->key_type == 0) { // 字符串键
                unicode_string* key_str = (unicode_string*)nova_memory_get_data(entry->str_key);
                
                // 转换字符串为系统编码
                char* sys_str = string_to_system(entry->str_key);
                if (sys_str) {
                    pos += sprintf(buffer + pos, "\"%s\": ", sys_str);
                    free(sys_str);
                } else {
                    pos += sprintf(buffer + pos, "\"[无法显示]\": ");
                }
            } else if (entry->key_type == 1) { // 整数键
                pos += sprintf(buffer + pos, "%ld: ", entry->int_key);
            } else if (entry->key_type == 2) { // 浮点数键
                pos += sprintf(buffer + pos, "%g: ", entry->float_key);
            }
            
            // 根据值类型格式化值
            switch (entry->val_type) {
                case 0: // 整数
                    pos += sprintf(buffer + pos, "%ld", entry->int_val);
                    break;
                case 1: // 浮点数
                    pos += sprintf(buffer + pos, "%g", entry->float_val);
                    break;
                case 2: // 布尔值
                    pos += sprintf(buffer + pos, "%s", entry->bool_val ? "True" : "False");
                    break;
                case 3: // 指针
                    pos += sprintf(buffer + pos, "<对象:0x%p>", entry->ptr_val);
                    break;
            }
        }
        
        // 检查冲突链
        nova_dict_entry* next_entry = entry->next;
        while (next_entry) {
            if (next_entry->is_occupied) {
                // 如果不是第一项，添加逗号
                if (!first_item) {
                    strcpy(buffer + pos, ", ");
                    pos += 2;
                } else {
                    first_item = false;
                }
                
                // 确保缓冲区有足够空间
                if (pos + 100 >= buf_size) {
                    buf_size *= 2;
                    char* new_buffer = (char*)realloc(buffer, buf_size);
                    if (!new_buffer) {
                        free(buffer);
                        return strdup("{内存不足}");
                    }
                    buffer = new_buffer;
                }
                
                // 根据键类型格式化键
                if (next_entry->key_type == 0) { // 字符串键
                    // 转换字符串为系统编码
                    char* sys_str = string_to_system(next_entry->str_key);
                    if (sys_str) {
                        pos += sprintf(buffer + pos, "\"%s\": ", sys_str);
                        free(sys_str);
                    } else {
                        pos += sprintf(buffer + pos, "\"[无法显示]\": ");
                    }
                } else if (next_entry->key_type == 1) { // 整数键
                    pos += sprintf(buffer + pos, "%ld: ", next_entry->int_key);
                } else if (next_entry->key_type == 2) { // 浮点数键
                    pos += sprintf(buffer + pos, "%g: ", next_entry->float_key);
                }
                
                // 根据值类型格式化值
                switch (next_entry->val_type) {
                    case 0: // 整数
                        pos += sprintf(buffer + pos, "%ld", next_entry->int_val);
                        break;
                    case 1: // 浮点数
                        pos += sprintf(buffer + pos, "%g", next_entry->float_val);
                        break;
                    case 2: // 布尔值
                        pos += sprintf(buffer + pos, "%s", next_entry->bool_val ? "True" : "False");
                        break;
                    case 3: // 指针
                        pos += sprintf(buffer + pos, "<对象:0x%p>", next_entry->ptr_val);
                        break;
                }
            }
            
            next_entry = next_entry->next;
        }
    }
    
    // 添加结束花括号
    buffer[pos++] = '}';
    buffer[pos] = '\0';
    
    return buffer;
}
