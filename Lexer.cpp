#include "Lexer.h"

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (pos < input.size()) {
        if (at_line_start) {
            current_indent = 0;
            skip_whitespace();
            if (peek() == '\n' || peek() == '#') {
                // 空行或注释不影响缩进
            } else if (current_indent > indent_levels.back()) {
                indent_levels.push_back(current_indent);
                tokens.emplace_back(TOK_INDENT, "", line);
            } else {
                while (current_indent < indent_levels.back()) {
                    indent_levels.pop_back();
                    tokens.emplace_back(TOK_DEDENT, "", line);
                }
                if (current_indent != indent_levels.back()) {
                    ctx.add_error(ErrorHandler::ErrorLevel::LEXICAL, "缩进错误", line, __FILE__, __LINE__);
                }
            }
            at_line_start = false;
        }

        char c = peek();
        if (c == ' ' || c == '\t') {
            pos++;
            continue;
        }
        
        if (c == '\n') {
            tokens.emplace_back(TOK_NEWLINE, "\n", line);
            pos++;
            line++;
            at_line_start = true;
            continue;
        }
        if (c == '#') {
            while (pos < input.size() && peek() != '\n') {
                pos++;
            }
            continue;
        }
        if (std::isalpha(c) || c == '_') {
            std::string id;
            while (pos < input.size() && (std::isalnum(peek()) || peek() == '_'))
                id += input[pos++];
            if (id == "def") tokens.emplace_back(TOK_DEF, id, line);
            else if (id == "return") tokens.emplace_back(TOK_RETURN, id, line);
            else if (id == "while") tokens.emplace_back(TOK_WHILE, id, line);
            else if (id == "True") tokens.emplace_back(TOK_TRUE, id, line);
            else if (id == "False") tokens.emplace_back(TOK_FALSE, id, line);
            else if (id == "and") tokens.emplace_back(TOK_AND, id, line);
            else if (id == "or") tokens.emplace_back(TOK_OR, id, line);
            else if (id == "not") tokens.emplace_back(TOK_NOT, id, line);
            else if (id == "print") tokens.emplace_back(TOK_PRINT, id, line);
            else if (id == "for") tokens.emplace_back(TOK_FOR, id, line);
            else if (id == "in") tokens.emplace_back(TOK_IN, id, line);
            else if (id == "range") tokens.emplace_back(TOK_RANGE, id, line);
            else if (id == "if") tokens.emplace_back(TOK_IF, id, line);
            else if (id == "else") tokens.emplace_back(TOK_ELSE, id, line);
            else if (id == "elif") tokens.emplace_back(TOK_ELIF, id, line);
            else if (id == "global") tokens.emplace_back(TOK_GLOBAL, id, line);
            else tokens.emplace_back(TOK_ID, id, line);
            continue;
        }
        if (std::isdigit(c) || c == '.' ||
            (c == '-' &&
             (tokens.back().type == TOK_PLUS || tokens.back().type == TOK_MINUS ||
              tokens.back().type == TOK_STAR ||
              tokens.back().type == TOK_SLASH || tokens.back().type == TOK_LT ||
              tokens.back().type == TOK_GTEQ || tokens.back().type == TOK_GT ||
              tokens.back().type == TOK_EQEQ ||
              tokens.back().type == TOK_LPAREN))) {
          std::string num;
          bool has_dot = false;
          while (pos < input.size() &&
                 (std::isdigit(peek()) || peek() == '.' || peek() == '-')) {
            if (peek() == '.')
              has_dot = true;
            num += input[pos++];
          }
          if (has_dot) {
            try {
              tokens.emplace_back(TOK_FLOAT, std::stod(num), line);
            } catch (...) {
              ctx.add_error(ErrorHandler::ErrorLevel::LEXICAL,
                            "无效的浮点数: " + num, line, __FILE__, __LINE__);
            }
          } else {
            try {
              tokens.emplace_back(TOK_INT, std::stol(num), line);
            } catch (...) {
              ctx.add_error(ErrorHandler::ErrorLevel::LEXICAL,
                            "无效的整数: " + num, line, __FILE__, __LINE__);
            }
          }
          continue;
        }
        if (c == '=') {
            pos++;
            if (peek() == '=') {
                tokens.emplace_back(TOK_EQEQ, "==", line);
                pos++;
            } else {
                tokens.emplace_back(TOK_ASSIGN, "=", line);
            }
            continue;
        }
        if (c == '+' && pos + 1 < input.size() && input[pos + 1] == '+') {
            tokens.emplace_back(TOK_PLUSPLUS, "++", line);
            pos += 2;
            continue;
        }
        if (c == '+' && pos + 1 < input.size() && input[pos + 1] == '=') {
            tokens.emplace_back(TOK_PLUSEQ, "+=", line);
            pos += 2;
            continue;
        }
        if (c == '+') {
            tokens.emplace_back(TOK_PLUS, "+", line);
            pos++;
            continue;
        }
        if (c == '-' && pos + 1 < input.size() && input[pos + 1] == '=') {
            tokens.emplace_back(TOK_MINUSEQ, "-=", line);
            pos += 2;
            continue;
        }
        if (c == '-') {
            tokens.emplace_back(TOK_MINUS, "-", line);
            pos++;
            continue;
        }
        if (c == '*' && pos + 1 < input.size() && input[pos + 1] == '=') {
            tokens.emplace_back(TOK_STAREQ, "*=", line);
            pos += 2;
            continue;
        }
        if (c == '*') {
            tokens.emplace_back(TOK_STAR, "*", line);
            pos++;
            continue;
        }
        if (c == '/' && pos + 1 < input.size() && input[pos + 1] == '=') {
            tokens.emplace_back(TOK_SLASHEQ, "/=", line);
            pos += 2;
            continue;
        }
        if (c == '/') {
            if (pos + 1 < input.size() && input[pos + 1] == '/') {
                tokens.emplace_back(TOK_SLASH, "//", line);
                pos += 2;
                continue;
            } else {
                tokens.emplace_back(TOK_SLASH, "/", line);
                pos++;
                continue;
            }
        }
        if (c == '<') {
            tokens.emplace_back(TOK_LT, "<", line);
            pos++;
            continue;
        }
        if (c == '>') {
            pos++;
            if (peek() == '=') {
                tokens.emplace_back(TOK_GTEQ, ">=", line);
                pos++;
            } else {
                tokens.emplace_back(TOK_GT, ">", line);
            }
            continue;
        }
        if (c == ':') {
            tokens.emplace_back(TOK_COLON, ":", line);
            pos++;
            continue;
        }
        if (c == '(') {
            tokens.emplace_back(TOK_LPAREN, "(", line);
            pos++;
            continue;
        }
        if (c == ')') {
            tokens.emplace_back(TOK_RPAREN, ")", line);
            pos++;
            continue;
        }
        if (c == ',') {
            tokens.emplace_back(TOK_COMMA, ",", line);
            pos++;
            continue;
        }
        ctx.add_error(ErrorHandler::ErrorLevel::LEXICAL,
                         "未知字符: " + std::string(1, c), line, __FILE__, __LINE__);
        pos++;
    }
    while (indent_levels.size() > 1) {
        indent_levels.pop_back();
        tokens.emplace_back(TOK_DEDENT, "", line);
    }
    tokens.emplace_back(TOK_EOF, "", line);
    return tokens;
}

void Lexer::print_tokens(const std::vector<Token>& tokens) {
    std::cout << "[Token 序列]\n";
    for (const auto& token : tokens) {
        std::cout << token_type_to_string(token.type) << " [行 " << token.line << "]";
        if (!token.value.empty()) {
            std::cout << ", \"" << token.value << "\"";
        } else if (token.type == TOK_INT) {
            std::cout << ", " << token.int_value;
        } else if (token.type == TOK_FLOAT) {
            std::cout << ", " << token.float_value;
        }
        std::cout << "\n";
    }
}

std::string Lexer::token_type_to_string(TokenType type) {
    switch (type) {
        case TOK_ID: return "TOK_ID";
        case TOK_INT: return "TOK_INT";
        case TOK_FLOAT: return "TOK_FLOAT";
        case TOK_ASSIGN: return "TOK_ASSIGN";
        case TOK_PLUSPLUS: return "TOK_PLUSPLUS";
        case TOK_PLUS: return "TOK_PLUS";
        case TOK_MINUS: return "TOK_MINUS";
        case TOK_STAR: return "TOK_STAR";
        case TOK_SLASH: return "TOK_SLASH";
        case TOK_LT: return "TOK_LT";
        case TOK_EQEQ: return "TOK_EQEQ";
        case TOK_GT: return "TOK_GT";
        case TOK_GTEQ: return "TOK_GTEQ";
        case TOK_TRUE: return "TOK_TRUE";
        case TOK_FALSE: return "TOK_FALSE";
        case TOK_AND: return "TOK_AND";
        case TOK_OR: return "TOK_OR";
        case TOK_NOT: return "TOK_NOT";
        case TOK_DEF: return "TOK_DEF";
        case TOK_RETURN: return "TOK_RETURN";
        case TOK_WHILE: return "TOK_WHILE";
        case TOK_COLON: return "TOK_COLON";
        case TOK_LPAREN: return "TOK_LPAREN";
        case TOK_RPAREN: return "TOK_RPAREN";
        case TOK_COMMA: return "TOK_COMMA";
        case TOK_NEWLINE: return "TOK_NEWLINE";
        case TOK_INDENT: return "TOK_INDENT";
        case TOK_DEDENT: return "TOK_DEDENT";
        case TOK_EOF: return "TOK_EOF";
        case TOK_PRINT: return "TOK_PRINT";
        case TOK_FOR: return "TOK_FOR";
        case TOK_IN: return "TOK_IN";
        case TOK_RANGE: return "TOK_RANGE";
        case TOK_IF: return "TOK_IF";
        case TOK_ELSE: return "TOK_ELSE";
        case TOK_ELIF: return "TOK_ELIF";
        case TOK_GLOBAL: return "TOK_GLOBAL";
        case TOK_PLUSEQ: return "TOK_PLUSEQ";
        case TOK_MINUSEQ: return "TOK_MINUSEQ";
        case TOK_STAREQ: return "TOK_STAREQ";
        case TOK_SLASHEQ: return "TOK_SLASHEQ";
        default: return "UNKNOWN";
    }
}
