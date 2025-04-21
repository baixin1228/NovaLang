#include "nova_string.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

// 从系统编码创建字符串
nova_memory_block* create_string_from_system(const char* str) {
    if (!str) return NULL;
    
    UErrorCode status = U_ZERO_ERROR;
    UConverter* conv = ucnv_open(NULL, &status); // 使用系统默认编码
    if (U_FAILURE(status)) return NULL;
    
    // 计算所需的Unicode字符串长度
    int32_t length = ucnv_toUChars(conv, NULL, 0, str, -1, &status);
    status = U_ZERO_ERROR;
    
    // 一次性分配结构体和字符数据的内存
    size_t data_size = sizeof(unicode_string) + length * sizeof(UChar);
    nova_memory_block* block = nova_memory_alloc(data_size);
    if (!block) {
        ucnv_close(conv);
        return NULL;
    }
    
    // 获取unicode_string指针
    unicode_string* ustr = (unicode_string*)nova_memory_get_data(block);
    
    // 设置长度
    ustr->length = length;
    
    // 转换字符串直接到结构体内的数据数组
    ucnv_toUChars(conv, ustr->data, length, str, -1, &status);
    ucnv_close(conv);
    
    if (U_FAILURE(status)) {
        nova_memory_release(block);
        return NULL;
    }
    
    return block;
}

// 从指定编码创建字符串
nova_memory_block* create_string_from_encoding(const char* str, const char* encoding) {
    if (!str || !encoding) return NULL;
    
    UErrorCode status = U_ZERO_ERROR;
    UConverter* conv = ucnv_open(encoding, &status);
    if (U_FAILURE(status)) return NULL;
    
    // 计算所需的Unicode字符串长度
    int32_t length = ucnv_toUChars(conv, NULL, 0, str, -1, &status);
    status = U_ZERO_ERROR;
    
    // 一次性分配结构体和字符数据的内存
    size_t data_size = sizeof(unicode_string) + length * sizeof(UChar);
    nova_memory_block* block = nova_memory_alloc(data_size);
    if (!block) {
        ucnv_close(conv);
        return NULL;
    }
    
    // 获取unicode_string指针
    unicode_string* ustr = (unicode_string*)nova_memory_get_data(block);
    
    // 设置长度
    ustr->length = length;
    
    // 转换字符串直接到结构体内的数据数组
    ucnv_toUChars(conv, ustr->data, length, str, -1, &status);
    ucnv_close(conv);
    
    if (U_FAILURE(status)) {
        nova_memory_release(block);
        return NULL;
    }
    
    return block;
}

// 从Unicode字符数组直接创建字符串
nova_memory_block* create_string_from_chars(const UChar* chars, int32_t length) {
    if (!chars || length <= 0) return NULL;
    
    // 分配内存：结构体大小 + Unicode字符数据大小
    size_t data_size = sizeof(unicode_string) + length * sizeof(UChar);
    nova_memory_block* block = nova_memory_alloc(data_size);
    if (!block) {
        return NULL;
    }
    
    // 获取unicode_string指针
    unicode_string* ustr = (unicode_string*)nova_memory_get_data(block);
    
    // 设置长度
    ustr->length = length;
    
    // 复制Unicode字符数据
    memcpy(ustr->data, chars, length * sizeof(UChar));
    
    return block;
}

// 将字符串转换为系统编码
char* string_to_system(const nova_memory_block* str) {
    if (!str) return NULL;
    
    // 获取unicode_string指针
    const unicode_string* ustr = (const unicode_string*)nova_memory_get_data(str);
    
    UErrorCode status = U_ZERO_ERROR;
    UConverter* conv = ucnv_open(NULL, &status); // 使用系统默认编码
    if (U_FAILURE(status)) return NULL;
    
    // 计算所需的目标字符串长度
    int32_t length = ucnv_fromUChars(conv, NULL, 0, ustr->data, ustr->length, &status);
    status = U_ZERO_ERROR;
    
    // 分配内存
    char* result = (char*)malloc((length + 1) * sizeof(char));
    if (!result) {
        ucnv_close(conv);
        return NULL;
    }
    
    // 转换字符串
    ucnv_fromUChars(conv, result, length + 1, ustr->data, ustr->length, &status);
    ucnv_close(conv);
    
    if (U_FAILURE(status)) {
        free(result);
        return NULL;
    }
    
    return result;
}

// 将字符串转换为指定编码
char* string_to_encoding(const nova_memory_block* str, const char* encoding) {
    if (!str || !encoding) return NULL;
    
    // 获取unicode_string指针
    const unicode_string* ustr = (const unicode_string*)nova_memory_get_data(str);
    
    UErrorCode status = U_ZERO_ERROR;
    UConverter* conv = ucnv_open(encoding, &status);
    if (U_FAILURE(status)) return NULL;
    
    // 计算所需的目标字符串长度
    int32_t length = ucnv_fromUChars(conv, NULL, 0, ustr->data, ustr->length, &status);
    status = U_ZERO_ERROR;
    
    // 分配内存
    char* result = (char*)malloc((length + 1) * sizeof(char));
    if (!result) {
        ucnv_close(conv);
        return NULL;
    }
    
    // 转换字符串
    ucnv_fromUChars(conv, result, length + 1, ustr->data, ustr->length, &status);
    ucnv_close(conv);
    
    if (U_FAILURE(status)) {
        free(result);
        return NULL;
    }
    
    return result;
}

// 打印字符串
void print_string(const nova_memory_block* str) {
    if (!str) return;
    
    // 获取unicode_string指针
    const unicode_string* ustr = (const unicode_string*)nova_memory_get_data(str);
    
    // 设置本地化以支持Unicode输出
    setlocale(LC_ALL, "");
    
    // 转换为系统编码并打印
    char* sys_str = string_to_system(str);
    if (sys_str) {
        printf("%s", sys_str);
        free(sys_str);
    }
}

// 打印字符串并换行
void println_string(const nova_memory_block* str) {
    print_string(str);
    printf("\n");
}

// 连接两个字符串
nova_memory_block* concat_strings(const nova_memory_block* str1, const nova_memory_block* str2) {
    if (!str1 || !str2) return NULL;
    
    // 获取unicode_string指针
    const unicode_string* ustr1 = (const unicode_string*)nova_memory_get_data(str1);
    const unicode_string* ustr2 = (const unicode_string*)nova_memory_get_data(str2);
    
    // 计算连接后的总长度
    int32_t total_length = ustr1->length + ustr2->length;
    
    // 分配新的内存，大小为结构体 + 字符数据
    size_t data_size = sizeof(unicode_string) + total_length * sizeof(UChar);
    nova_memory_block* result_block = nova_memory_alloc(data_size);
    if (!result_block) return NULL;
    
    // 获取结果的unicode_string指针
    unicode_string* result = (unicode_string*)nova_memory_get_data(result_block);
    
    // 设置长度
    result->length = total_length;
    
    // 复制第一个字符串的内容
    memcpy(result->data, ustr1->data, ustr1->length * sizeof(UChar));
    
    // 复制第二个字符串的内容（紧接着第一个字符串后面）
    memcpy(result->data + ustr1->length, ustr2->data, ustr2->length * sizeof(UChar));
    
    return result_block;
}

// 获取字符串长度
int32_t get_string_length(const nova_memory_block* str) {
    if (!str) return 0;
    
    // 获取unicode_string指针
    const unicode_string* ustr = (const unicode_string*)nova_memory_get_data(str);
    return ustr->length;
} 