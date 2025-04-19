#include "runtime.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

// 从系统编码创建Unicode字符串
unicode_string* create_unicode_string_from_system(const char* str) {
    if (!str) return NULL;
    
    UErrorCode status = U_ZERO_ERROR;
    UConverter* conv = ucnv_open(NULL, &status); // 使用系统默认编码
    if (U_FAILURE(status)) return NULL;
    
    // 计算所需的Unicode字符串长度
    int32_t length = ucnv_toUChars(conv, NULL, 0, str, -1, &status);
    status = U_ZERO_ERROR;
    
    // 一次性分配结构体和字符数据的内存
    unicode_string* ustr = (unicode_string*)malloc(sizeof(unicode_string) + length * sizeof(UChar));
    if (!ustr) {
        ucnv_close(conv);
        return NULL;
    }
    
    // 设置长度
    ustr->length = length;
    
    // 转换字符串直接到结构体内的数据数组
    ucnv_toUChars(conv, ustr->data, length, str, -1, &status);
    ucnv_close(conv);
    
    if (U_FAILURE(status)) {
        free(ustr);
        return NULL;
    }
    
    return ustr;
}

// 从指定编码创建Unicode字符串
unicode_string* create_unicode_string_from_encoding(const char* str, const char* encoding) {
    if (!str || !encoding) return NULL;
    
    UErrorCode status = U_ZERO_ERROR;
    UConverter* conv = ucnv_open(encoding, &status);
    if (U_FAILURE(status)) return NULL;
    
    // 计算所需的Unicode字符串长度
    int32_t length = ucnv_toUChars(conv, NULL, 0, str, -1, &status);
    status = U_ZERO_ERROR;
    
    // 一次性分配结构体和字符数据的内存
    unicode_string* ustr = (unicode_string*)malloc(sizeof(unicode_string) + length * sizeof(UChar));
    if (!ustr) {
        ucnv_close(conv);
        return NULL;
    }
    
    // 设置长度
    ustr->length = length;
    
    // 转换字符串直接到结构体内的数据数组
    ucnv_toUChars(conv, ustr->data, length, str, -1, &status);
    ucnv_close(conv);
    
    if (U_FAILURE(status)) {
        free(ustr);
        return NULL;
    }
    
    return ustr;
}

// 将Unicode字符串转换为系统编码
char* unicode_string_to_system(const unicode_string* str) {
    if (!str) return NULL;
    
    UErrorCode status = U_ZERO_ERROR;
    UConverter* conv = ucnv_open(NULL, &status); // 使用系统默认编码
    if (U_FAILURE(status)) return NULL;
    
    // 计算所需的目标字符串长度
    int32_t length = ucnv_fromUChars(conv, NULL, 0, str->data, str->length, &status);
    status = U_ZERO_ERROR;
    
    // 分配内存
    char* result = (char*)malloc((length + 1) * sizeof(char));
    if (!result) {
        ucnv_close(conv);
        return NULL;
    }
    
    // 转换字符串
    ucnv_fromUChars(conv, result, length + 1, str->data, str->length, &status);
    ucnv_close(conv);
    
    if (U_FAILURE(status)) {
        free(result);
        return NULL;
    }
    
    return result;
}

// 将Unicode字符串转换为指定编码
char* unicode_string_to_encoding(const unicode_string* str, const char* encoding) {
    if (!str || !encoding) return NULL;
    
    UErrorCode status = U_ZERO_ERROR;
    UConverter* conv = ucnv_open(encoding, &status);
    if (U_FAILURE(status)) return NULL;
    
    // 计算所需的目标字符串长度
    int32_t length = ucnv_fromUChars(conv, NULL, 0, str->data, str->length, &status);
    status = U_ZERO_ERROR;
    
    // 分配内存
    char* result = (char*)malloc((length + 1) * sizeof(char));
    if (!result) {
        ucnv_close(conv);
        return NULL;
    }
    
    // 转换字符串
    ucnv_fromUChars(conv, result, length + 1, str->data, str->length, &status);
    ucnv_close(conv);
    
    if (U_FAILURE(status)) {
        free(result);
        return NULL;
    }
    
    return result;
}

// 释放Unicode字符串 - 现在只需要一次free调用
void free_unicode_string(unicode_string* str) {
    // 释放整个结构体，包括柔性数组部分
    free(str);
}

// 打印Unicode字符串
void print_unicode_string(const unicode_string* str) {
    if (!str) return;
    
    // 设置本地化以支持Unicode输出
    setlocale(LC_ALL, "");
    
    // 转换为系统编码并打印
    char* sys_str = unicode_string_to_system(str);
    if (sys_str) {
        printf("%s", sys_str);
        free(sys_str);
    }
}

// 打印Unicode字符串并换行
void println_unicode_string(const unicode_string* str) {
    print_unicode_string(str);
    printf("\n");
} 