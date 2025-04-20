#pragma once
#include "Context.h"
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "Error.h"

// Token 类型
enum TokenType {
    TOK_ID, TOK_INT, TOK_FLOAT, TOK_ASSIGN, TOK_PLUSPLUS, TOK_PLUS, TOK_MINUS,
    TOK_STAR, TOK_SLASH, TOK_DSLASH, TOK_LT, TOK_EQEQ, TOK_GT, TOK_GTEQ, TOK_TRUE, TOK_FALSE,
    TOK_AND, TOK_OR, TOK_NOT, TOK_DEF, TOK_RETURN, TOK_WHILE, TOK_COLON,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_LBRACKET, TOK_RBRACKET,
    TOK_COMMA, TOK_NEWLINE, TOK_INDENT, TOK_DEDENT, TOK_DOT,
    TOK_EOF, TOK_PRINT, TOK_FOR, TOK_IN, TOK_RANGE, TOK_IF, TOK_ELSE, TOK_ELIF,
    TOK_GLOBAL, TOK_PLUSEQ, TOK_MINUSEQ, TOK_STAREQ, TOK_SLASHEQ, TOK_STRING
};

struct Token {
    TokenType type;
    std::string value;
    long int_value = 0;
    double float_value = 0.0;
    std::string string_value;
    int line = 0;

    Token(TokenType t, std::string v = "", int ln = 0) : type(t), value(v), line(ln) {}
    Token(TokenType t, long i, int ln) : type(t), int_value(i), line(ln) {}
    Token(TokenType t, double f, int ln) : type(t), float_value(f), line(ln) {}
    Token(TokenType t, const std::string& s, int ln, bool is_string) 
        : type(t), string_value(s), line(ln) {}
};

// 词法分析
class Lexer {
    std::string input;
    size_t pos = 0;
    std::vector<int> indent_levels{0};
    int current_indent = 0;
    bool at_line_start = true;
    int line = 1;
    Context& ctx;

    char peek() { return pos < input.size() ? input[pos] : '\0'; }

    void skip_whitespace() {
        while (pos < input.size() && (std::isspace(peek()) && peek() != '\n')) {
            if (at_line_start && peek() == ' ') {
                current_indent++;
            }
            pos++;
        }
    }

public:
  Lexer(Context &ctx, const std::string &src) : ctx(ctx), input(src) {}
  std::vector<Token> tokenize();
  void print_tokens(const std::vector<Token> &tokens);

private:
    std::string token_type_to_string(TokenType type);
};
