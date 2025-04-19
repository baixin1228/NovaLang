#include "runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    // 测试基本的ASCII字符
    unsigned int ascii_data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    unicode_string* ascii_str = create_unicode_string(ascii_data, 11);
    printf("Testing ASCII string: ");
    println_unicode_string(ascii_str);
    
    // 测试中文字符
    unsigned int chinese_data[] = {0x4F60, 0x597D, 0x4E16, 0x754C}; // "你好世界"
    unicode_string* chinese_str = create_unicode_string(chinese_data, 4);
    printf("Testing Chinese string: ");
    println_unicode_string(chinese_str);
    
    // 测试特殊Unicode字符
    unsigned int special_data[] = {0x1F600}; // 笑脸emoji
    unicode_string* special_str = create_unicode_string(special_data, 1);
    printf("Testing Emoji: ");
    println_unicode_string(special_str);
    
    // 测试UTF-8转换
    printf("\nTesting UTF-8 conversion:\n");
    const char* utf8_str = "Hello, 世界! 😀";
    unicode_string* converted_str = create_unicode_string_from_utf8(utf8_str);
    printf("Original UTF-8 string: %s\n", utf8_str);
    printf("Converted and printed back: ");
    println_unicode_string(converted_str);
    
    // 测试转换回UTF-8
    char* back_to_utf8 = unicode_string_to_utf8(converted_str);
    printf("Converted back to UTF-8: %s\n", back_to_utf8);
    
    // 清理
    free_unicode_string(ascii_str);
    free_unicode_string(chinese_str);
    free_unicode_string(special_str);
    free_unicode_string(converted_str);
    free(back_to_utf8);
    
    return 0;
} 