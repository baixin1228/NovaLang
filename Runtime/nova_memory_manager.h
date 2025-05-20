#ifndef NOVA_MEMORY_MANAGER_H
#define NOVA_MEMORY_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

// 内存块结构体，包含引用计数
typedef struct {
    int32_t ref_count; // 引用计数
    // 数据紧跟在这个结构体后面
    char data[];       // 柔性数组成员，存储实际数据
} nova_memory_block;

/**
 * 分配指定大小的内存块
 * 
 * @param size 需要分配的数据大小（不包括引用计数器的大小）
 * @return 指向分配的内存块的指针，NULL表示失败
 */
nova_memory_block* nova_memory_alloc(size_t size);

/**
 * 增加内存块的引用计数
 * 
 * @param block 内存块指针
 * @return 增加后的引用计数值
 */
int32_t nova_memory_retain(nova_memory_block* block);

/**
 * 减少内存块的引用计数，当引用计数为0时释放内存
 * 
 * @param block 内存块指针
 * @return 减少后的引用计数值，如果内存被释放则返回0
 */
int32_t nova_memory_release(nova_memory_block* block);

/**
 * 获取指向内存块数据部分的指针
 * 
 * @param block 内存块指针
 * @return 指向数据部分的指针
 */
void* nova_memory_get_data(nova_memory_block* block);

/**
 * 复制内存数据，从一个内存块到另一个内存块
 * 
 * @param dst 目标内存块
 * @param src 源内存块
 * @param size 要复制的字节数
 * @return 指向目标内存块的指针
 */
nova_memory_block* nova_memory_copy(nova_memory_block* dst, nova_memory_block* src, size_t size);

#ifdef __cplusplus
}
#endif

#endif // NOVA_MEMORY_MANAGER_H 