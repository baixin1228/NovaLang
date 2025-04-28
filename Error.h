#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <fstream>
#include <stdexcept>
#include <sstream>

// 错误处理类
class ErrorHandler {
public:
    enum class ErrorLevel {
        LEXICAL,
        SYNTAX,
        TYPE,
        INTERNAL,
        RUNTIME,
        Other
    };

    struct Error {
        ErrorLevel level;
        std::string message;
        int source_line;     // 源代码中的行号
        std::string file;    // 调用位置的文件名
        int call_line;       // 调用位置的行号
        
        Error(ErrorLevel l, std::string m, int src_ln, const char* f, int c_ln) 
            : level(l), message(m), source_line(src_ln), file(f), call_line(c_ln) {}
    };

private:
    std::vector<Error> errors;

public:
    void add_error(ErrorLevel level, const std::string& message, int line, const char* file, int call_line);
    void add_error_front(ErrorLevel level, const std::string& message, int line, const char* file, int call_line);

    bool has_errors() const { return !errors.empty(); }

    void print_errors() const;
    
    // 获取错误字符串
    std::string get_error_string() const;
};