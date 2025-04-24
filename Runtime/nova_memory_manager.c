#include "nova_memory_manager.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if DEBUG
#define DEBUG_MEMORY
#endif

// 调试信息打印宏
#ifdef DEBUG_MEMORY
#define DEBUG_PRINT(fmt, ...) fprintf(stderr, "[MEMORY] " fmt "\n", ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

// 简单的内存块哈希表定义
#define HASH_TABLE_SIZE 1024
typedef struct {
    nova_memory_block* blocks[HASH_TABLE_SIZE];
    size_t count;
} nova_memory_table;

// 全局哈希表，用于跟踪引用计数为0的内存块
static nova_memory_table* zero_ref_blocks = NULL;

// 哈希函数
static size_t hash_ptr(void* ptr) {
    return ((size_t)ptr) % HASH_TABLE_SIZE;
}

// 初始化内存管理系统
void nova_memory_init() {
    // 创建哈希表
    zero_ref_blocks = (nova_memory_table*)malloc(sizeof(nova_memory_table));
    if (zero_ref_blocks) {
        memset(zero_ref_blocks->blocks, 0, sizeof(zero_ref_blocks->blocks));
        zero_ref_blocks->count = 0;
        DEBUG_PRINT("Memory manager initialized");
    }
}

// 清理内存管理系统
void nova_memory_cleanup() {
    if (zero_ref_blocks != NULL) {
        // 释放哈希表中所有剩余的内存块
        for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
            if (zero_ref_blocks->blocks[i] != NULL) {
                DEBUG_PRINT("Cleaning up unreferenced block %p", zero_ref_blocks->blocks[i]);
                free(zero_ref_blocks->blocks[i]);
            }
        }
        
        // 销毁哈希表
        free(zero_ref_blocks);
        zero_ref_blocks = NULL;
        DEBUG_PRINT("Memory manager cleaned up");
    }
}

// 添加块到哈希表
static void add_to_table(nova_memory_block* block) {
    if (zero_ref_blocks == NULL || block == NULL) {
        return;
    }
    
    size_t index = hash_ptr(block);
    
    // 简单的线性探测解决冲突
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        size_t pos = (index + i) % HASH_TABLE_SIZE;
        if (zero_ref_blocks->blocks[pos] == NULL) {
            zero_ref_blocks->blocks[pos] = block;
            zero_ref_blocks->count++;
            DEBUG_PRINT("Added block %p to zero_ref_blocks at index %zu", block, pos);
            return;
        }
    }
    
    // 如果哈希表已满，直接释放内存
    DEBUG_PRINT("Zero ref table full, freeing block %p immediately", block);
    free(block);
}

// 从哈希表中移除块
static int remove_from_table(nova_memory_block* block) {
    if (zero_ref_blocks == NULL || block == NULL) {
        return 0;
    }
    
    size_t index = hash_ptr(block);
    
    // 线性探测查找元素
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        size_t pos = (index + i) % HASH_TABLE_SIZE;
        if (zero_ref_blocks->blocks[pos] == block) {
            zero_ref_blocks->blocks[pos] = NULL;
            zero_ref_blocks->count--;
            DEBUG_PRINT("Removed block %p from zero_ref_blocks at index %zu", block, pos);
            return 1;
        }
        
        // 如果遇到空槽且之前未找到元素，则表示元素不在表中
        if (zero_ref_blocks->blocks[pos] == NULL) {
            break;
        }
    }
    
    return 0;
}

// 检查块是否在哈希表中
static int is_in_table(nova_memory_block* block) {
    if (zero_ref_blocks == NULL || block == NULL) {
        return 0;
    }
    
    size_t index = hash_ptr(block);
    
    // 线性探测查找元素
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        size_t pos = (index + i) % HASH_TABLE_SIZE;
        if (zero_ref_blocks->blocks[pos] == block) {
            return 1;
        }
        
        // 如果遇到空槽且之前未找到元素，则表示元素不在表中
        if (zero_ref_blocks->blocks[pos] == NULL) {
            break;
        }
    }
    
    return 0;
}

nova_memory_block* nova_memory_alloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    // 确保哈希表已初始化
    if (zero_ref_blocks == NULL) {
        nova_memory_init();
    }
    
    // 分配内存：结构体大小 + 请求的数据大小
    nova_memory_block* block = (nova_memory_block*)malloc(sizeof(nova_memory_block) + size);
    if (block == NULL) {
        DEBUG_PRINT("Failed to allocate memory of size %zu", size);
        return NULL;
    }
    
    // 初始化引用计数为0
    block->ref_count = 0;
    
    // 清零数据区域（可选）
    // memset(block->data, 0, size);
    
    // 将新分配的内存块添加到哈希表中
    add_to_table(block);
    
    DEBUG_PRINT("Allocated memory block %p with size %zu (added to zero_ref_blocks)", block, size);
    return block;
}

int32_t nova_memory_retain(nova_memory_block* block) {
    if (block == NULL) {
        DEBUG_PRINT("Attempt to retain NULL block");
        return 0;
    }
    
    // 增加引用计数
    block->ref_count++;
    
    // 如果这是第一次引用，从零引用哈希表中移除
    if (block->ref_count == 1) {
        remove_from_table(block);
    }
    
    DEBUG_PRINT("Retained block %p, new ref count: %d", block, block->ref_count);
    return block->ref_count;
}

void nova_memory_collect_garbage();

  int32_t nova_memory_release(nova_memory_block * block) {
    if (block == NULL) {
        DEBUG_PRINT("Attempt to release NULL block");
        return 0;
    }
    
    // 减少引用计数
    block->ref_count--;
    
    DEBUG_PRINT("Released block %p, new ref count: %d", block, block->ref_count);
    
    // 如果引用计数为0，直接释放内存
    if (block->ref_count <= 0) {
        DEBUG_PRINT("Freeing block %p", block);
        
        // 如果在哈希表中，先移除
        if (is_in_table(block)) {
            printf("Freeing block %p from zero_ref_blocks\n", block);
            exit(-1);
        }
        
        free(block);
        nova_memory_collect_garbage();

        return 0;
    }
    
    return block->ref_count;
}

void* nova_memory_get_data(nova_memory_block* block) {
    if (block == NULL) {
        return NULL;
    }
    
    // 返回指向数据部分的指针
    return block->data;
}

// 清理所有未引用内存块
void nova_memory_collect_garbage() {
    if (zero_ref_blocks == NULL || zero_ref_blocks->count == 0) {
        DEBUG_PRINT("No garbage to collect");
        return;
    }
    
    size_t count = zero_ref_blocks->count;
    DEBUG_PRINT("Collecting %zu unreferenced memory blocks", count);
    
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        if (zero_ref_blocks->blocks[i] != NULL) {
            DEBUG_PRINT("Freeing unreferenced block %p", zero_ref_blocks->blocks[i]);
            free(zero_ref_blocks->blocks[i]);
            zero_ref_blocks->blocks[i] = NULL;
        }
    }
    
    zero_ref_blocks->count = 0;
    DEBUG_PRINT("Garbage collection completed, freed %zu blocks", count);
}