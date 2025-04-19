#include "StringLiteral.h"
#include "TypeChecker.h"
#include "ASTParser.h"
#include <iostream>

icu::UnicodeString StringLiteral::convertToUnicode(const std::string& str, const char* encoding) {
    UErrorCode status = U_ZERO_ERROR;
    UConverter* converter = ucnv_open(encoding, &status);
    if (U_FAILURE(status) || !converter) {
        throw std::runtime_error("Failed to create converter for " + std::string(encoding));
    }

    // Create UnicodeString from the input string using the specified converter
    icu::UnicodeString result(str.c_str(), str.length(), converter, status);
    ucnv_close(converter);

    if (U_FAILURE(status)) {
        throw std::runtime_error("Failed to convert string to Unicode");
    }

    return result;
}

std::string StringLiteral::convertFromUnicode(const icu::UnicodeString& str, const char* encoding) {
    UErrorCode status = U_ZERO_ERROR;
    UConverter* converter = ucnv_open(encoding, &status);
    if (U_FAILURE(status) || !converter) {
        throw std::runtime_error("Failed to create converter for " + std::string(encoding));
    }

    // Calculate the maximum size needed for the output buffer
    int32_t targetLength = str.length() * ucnv_getMaxCharSize(converter);
    std::string result;
    result.resize(targetLength);

    // Convert the Unicode string to the target encoding
    char* target = &result[0];
    const UChar* source = str.getBuffer();
    int32_t sourceLength = str.length();
    
    ucnv_fromUChars(converter, target, targetLength, source, sourceLength, &status);
    ucnv_close(converter);

    if (U_FAILURE(status)) {
        throw std::runtime_error("Failed to convert Unicode to target encoding");
    }

    // Resize the string to the actual converted length
    result.resize(strlen(result.c_str()));
    return result;
}

StringLiteral::StringLiteral(Context& ctx, const std::string& str, int ln, const char* sourceEncoding)
    : ASTNode(ctx, ln), raw_str(str) {  // 保存原始字符串
    unicode_value = convertToUnicode(str, sourceEncoding);
}

void StringLiteral::print(int level) {
    std::cout << std::string(level * 2, ' ') << "StringLiteral: ";
    std::string utf8Str = convertFromUnicode(unicode_value, "UTF-8");
    std::cout << utf8Str << " [行 " << line << "]\n";
}

std::string StringLiteral::to_utf8() const {
    return convertFromUnicode(unicode_value, "UTF-8");
}

std::string StringLiteral::to_utf16() const {
    return convertFromUnicode(unicode_value, "UTF-16");
}

std::string StringLiteral::to_utf32() const {
    return convertFromUnicode(unicode_value, "UTF-32");
}

std::string StringLiteral::to_gbk() const {
    return convertFromUnicode(unicode_value, "GBK");
}

std::string StringLiteral::to_big5() const {
    return convertFromUnicode(unicode_value, "Big5");
}

std::string StringLiteral::to_encoding(const char* encoding) const {
    return convertFromUnicode(unicode_value, encoding);
}

int StringLiteral::visit_stmt(VarType &result) {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "字符串字面量不能作为语句使用", line, __FILE__, __LINE__);
    return -1;
}

int StringLiteral::visit_expr(VarType &result) {
    result = VarType::STRING;
    return 0;
} 