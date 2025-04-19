#ifndef RUNTIME_H
#define RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <unicode/ustring.h>
#include <unicode/ucnv.h>

// Unicode字符串结构 - 使用柔性数组成员模式
typedef struct {
    int32_t length;   // 字符数量
    UChar data[];     // 柔性数组成员，直接存储Unicode字符数据
} unicode_string;

// 从系统编码创建Unicode字符串
unicode_string* create_unicode_string_from_system(const char* str);

// 从指定编码创建Unicode字符串
unicode_string* create_unicode_string_from_encoding(const char* str, const char* encoding);

// 将Unicode字符串转换为系统编码
char* unicode_string_to_system(const unicode_string* str);

// 将Unicode字符串转换为指定编码
char* unicode_string_to_encoding(const unicode_string* str, const char* encoding);

// 释放Unicode字符串
void free_unicode_string(unicode_string* str);

// 打印Unicode字符串
void print_unicode_string(const unicode_string* str);

// 打印Unicode字符串并换行
void println_unicode_string(const unicode_string* str);

// 连接两个Unicode字符串
unicode_string* concat_unicode_strings(const unicode_string* str1, const unicode_string* str2);

#ifdef __cplusplus
}
#endif

#endif // RUNTIME_H 