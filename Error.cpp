#include "Error.h"

void ErrorHandler::add_error(ErrorLevel level, const std::string& message, int line, const char* file, int call_line) {
    errors.emplace_back(level, message, line, file, call_line);
}

void ErrorHandler::add_error_front(ErrorLevel level, const std::string& message, int line, const char* file, int call_line) {
    errors.insert(errors.begin(), Error(level, message, line, file, call_line));
}

std::string ErrorHandler::get_error_string() const {
    std::stringstream ss;
    ss << "[错误列表]\n";
    for (const auto& err : errors) {
        std::string level_str;
        switch (err.level) {
            case ErrorLevel::LEXICAL:
                level_str = "词法错误";
                break;
            case ErrorLevel::SYNTAX:
                level_str = "语法错误";
                break;
            case ErrorLevel::TYPE:
                level_str = "类型错误";
                break;
            case ErrorLevel::Other:
                break;
        }
        
        // 获取文件的基本名称（去掉路径）
        std::string filename = err.file;
        size_t last_slash = filename.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            filename = filename.substr(last_slash + 1);
        }
        
        switch (err.level) {
            case ErrorLevel::LEXICAL:
            case ErrorLevel::SYNTAX:
            case ErrorLevel::TYPE:
                ss << level_str << " [源码行 " << err.source_line << "] [" 
                   << filename << ":" << err.call_line << "]: " 
                   << err.message << "\n";
                break;
            case ErrorLevel::Other:
                ss << err.message << " [" << filename << ":" << err.call_line << "]\n";
                break;
        }
    }
    return ss.str();
}

void ErrorHandler::print_errors() const {
    std::cout << get_error_string();
}
