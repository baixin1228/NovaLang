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

int StringLiteral::visit_stmt() {
    ctx.add_error(ErrorHandler::ErrorLevel::TYPE, "字符串字面量不能作为语句使用", line, __FILE__, __LINE__);
    return -1;
}

int StringLiteral::visit_expr(std::shared_ptr<ASTNode> &self) {
    type = VarType::STRING;
    self = shared_from_this();
    return 0;
}

int StringLiteral::gencode_stmt() { return 0; }

std::map<std::string, llvm::Value *> string_pool;
llvm::Value *StringLiteral::gencode_expr(VarType expected_type) {
  // 获取原始字符串作为key
  const std::string &str_key = get_raw_str();

  // 检查字符串池中是否已存在
  auto it = string_pool.find(str_key);
  if (it != string_pool.end()) {
    return it->second; // 返回已存在的内存指针
  }

  // 获取Unicode字符串
  const icu::UnicodeString &unicode_str = get_unicode();

  // 创建包含Unicode数据的全局常量数组
  llvm::Constant *chars_array = nullptr;
  std::vector<llvm::Constant *> chars;
  for (int i = 0; i < unicode_str.length(); i++) {
    chars.push_back(
        llvm::ConstantInt::get(ctx.builder->getInt16Ty(), unicode_str.charAt(i)));
  }

  // 创建UChar数组类型和常量数组
  llvm::ArrayType *char_array_type =
      llvm::ArrayType::get(ctx.builder->getInt16Ty(), chars.size());
  chars_array = llvm::ConstantArray::get(char_array_type, chars);

  // 将数组存储为全局变量以便使用
  std::string array_name =
      "unicode_chars_" + std::to_string(string_pool.size());
  auto global_array = new llvm::GlobalVariable(
      *ctx.module, char_array_type,
      true, // 是常量
      llvm::GlobalValue::PrivateLinkage, chars_array, array_name);

  // 创建指向数组的指针
  llvm::Value *array_ptr = ctx.builder->CreateBitCast(
      global_array, llvm::PointerType::get(ctx.builder->getInt16Ty(), 0),
      "unicode_data_ptr");

  // 使用运行时库函数创建Unicode字符串
  auto create_func =
      ctx.runtime_manager->getRuntimeFunction("create_string_from_chars");
  llvm::Value *length =
      llvm::ConstantInt::get(ctx.builder->getInt32Ty(), unicode_str.length());
  llvm::Value *memory_block =
      ctx.builder->CreateCall(create_func, {array_ptr, length}, "create_string");

  // 将指针存入字符串池
  string_pool[str_key] = memory_block;

  return memory_block;
}