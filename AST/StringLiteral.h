#pragma once
#include "ASTNode.h"
#include "Context.h"
#include <string>
#include <unicode/unistr.h>
#include <unicode/ucnv.h>

class StringLiteral : public ASTNode {
private:
    icu::UnicodeString unicode_value;
    std::string raw_str;  // 存储原始字符串，用于字符串池的key

    // Convert from any encoding to Unicode
    static icu::UnicodeString convertToUnicode(const std::string& str, const char* encoding);

    // Convert from Unicode to any encoding
    static std::string convertFromUnicode(const icu::UnicodeString& str, const char* encoding);

public:
    StringLiteral(Context& ctx, const std::string& str, int ln, const char* sourceEncoding = "UTF-8");

    const std::string& get_raw_str() const { return raw_str; }

    void print(int level = 0) override;

    // Convert to various encodings
    std::string to_utf8() const;
    std::string to_utf16() const;
    std::string to_utf32() const;
    std::string to_gbk() const;
    std::string to_big5() const;
    std::string to_encoding(const char* encoding) const;

    // Get raw Unicode string
    const icu::UnicodeString& get_unicode() const { return unicode_value; }

    int visit_stmt() override;
    int visit_expr(std::shared_ptr<ASTNode> &self) override;
    int gencode_stmt() override;
    llvm::Value *gencode_expr(VarType expected_type) override;
};