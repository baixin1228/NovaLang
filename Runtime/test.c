#include "runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    // 测试基本的ASCII字符
    UChar ascii_data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    nova_memory_block* ascii_str = create_string_from_chars(ascii_data, 11);
    printf("Testing ASCII string: ");
    println_string(ascii_str);
    
    // 测试中文字符
    UChar chinese_data[] = {0x4F60, 0x597D, 0x4E16, 0x754C}; // "你好世界"
    nova_memory_block* chinese_str = create_string_from_chars(chinese_data, 4);
    printf("Testing Chinese string: ");
    println_string(chinese_str);
    
    // 测试特殊Unicode字符
    UChar special_data[] = {0x1F600}; // 笑脸emoji
    nova_memory_block* special_str = create_string_from_chars(special_data, 1);
    printf("Testing Emoji: ");
    println_string(special_str);
    
    // 测试UTF-8转换
    printf("\nTesting UTF-8 conversion:\n");
    const char* utf8_str = "Hello, 世界! 😀";
    nova_memory_block* converted_str = create_string_from_system(utf8_str);
    printf("Original UTF-8 string: %s\n", utf8_str);
    printf("Converted and printed back: ");
    println_string(converted_str);
    
    // 测试转换回UTF-8
    char* back_to_utf8 = string_to_system(converted_str);
    printf("Converted back to UTF-8: %s\n", back_to_utf8);
    
    // 清理
    nova_memory_release(ascii_str);
    nova_memory_release(chinese_str);
    nova_memory_release(special_str);
    nova_memory_release(converted_str);
    free(back_to_utf8);
    
    return 0;
} 