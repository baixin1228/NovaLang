#ifndef RUNTIME_H
#define RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <unicode/ustring.h>
#include <unicode/ucnv.h>
#include "nova_memory_manager.h"

// Unicode字符串结构 - 使用柔性数组成员模式
typedef struct {
    int32_t length;   // 字符数量
    UChar data[];     // 柔性数组成员，直接存储Unicode字符数据
} unicode_string;

// 从系统编码创建字符串
nova_memory_block* create_string_from_system(const char* str);

// 从指定编码创建字符串
nova_memory_block* create_string_from_encoding(const char* str, const char* encoding);

// 从Unicode字符数组直接创建字符串
nova_memory_block* create_string_from_chars(const UChar* chars, int32_t length);

// 将字符串转换为系统编码
char* string_to_system(const nova_memory_block* str);

// 将字符串转换为指定编码
char* string_to_encoding(const nova_memory_block* str, const char* encoding);

// 打印字符串
void print_string(const nova_memory_block* str);

// 打印字符串并换行
void println_string(const nova_memory_block* str);

// 连接两个字符串
nova_memory_block* concat_strings(const nova_memory_block* str1, const nova_memory_block* str2);

// 获取字符串长度
int32_t get_string_length(const nova_memory_block* str);

#ifdef __cplusplus
}
#endif

#endif // RUNTIME_H 