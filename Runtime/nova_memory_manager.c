#include "nova_memory_manager.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DEBUG_MEMORY
// 调试信息打印宏
#ifdef DEBUG_MEMORY
#define DEBUG_PRINT(fmt, ...) fprintf(stderr, "[MEMORY] " fmt "\n", ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

nova_memory_block* nova_memory_alloc(size_t size) {
    if (size == 0) {
        return NULL;
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
    
    DEBUG_PRINT("Allocated memory block %p with size %zu", block, size);
    return block;
}

int32_t nova_memory_retain(nova_memory_block* block) {
    if (block == NULL) {
        DEBUG_PRINT("Attempt to retain NULL block");
        return 0;
    }
    
    // 增加引用计数
    block->ref_count++;
    
    DEBUG_PRINT("Retained block %p, new ref count: %d", block, block->ref_count);
    return block->ref_count;
}

int32_t nova_memory_release(nova_memory_block* block) {
    if (block == NULL) {
        DEBUG_PRINT("Attempt to release NULL block");
        return 0;
    }
    
    // 减少引用计数
    block->ref_count--;
    
    DEBUG_PRINT("Released block %p, new ref count: %d", block, block->ref_count);
    
    // 如果引用计数为0，释放内存
    if (block->ref_count <= 0) {
        DEBUG_PRINT("Freeing block %p", block);
        free(block);
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