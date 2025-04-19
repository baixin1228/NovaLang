#pragma once
#include "ASTNode.h"
#include "Context.h"
#include "Error.h"
#include "Lexer.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// AST 解析器
class ASTParser {
  std::vector<Token> tokens;
  size_t pos = 0;
  Context &ctx;

  Token &current() { return tokens[pos]; }
  void consume(TokenType type, std::string file, int line) {
    if (current().type != type) {
      // ctx.add_error(ErrorHandler::ErrorLevel::SYNTAX,
      //               "期望 " + token_type_to_string(type) + " 得到 " +
      //                   token_type_to_string(current().type),
      //               current().line, file.c_str(), line);
        throw std::runtime_error("期望 " + token_type_to_string(type) + " 得到 " +
                                    token_type_to_string(current().type) + 
                                    " source_code:" + std::to_string(current().line) + " file:" + file + " line:" + std::to_string(line));
    }
    pos++;
  }

  std::string token_type_to_string(TokenType type) {
    switch (type) {
    case TOK_INT:
      return "TOK_INT";
    case TOK_FLOAT:
      return "TOK_FLOAT";
    case TOK_ID:
      return "TOK_ID";
    case TOK_PLUS:
      return "TOK_PLUS";
    case TOK_MINUS:
      return "TOK_MINUS";
    case TOK_STAR:
      return "TOK_STAR";
    case TOK_SLASH:
      return "TOK_SLASH";
    case TOK_LPAREN:
      return "TOK_LPAREN";
    case TOK_RPAREN:
      return "TOK_RPAREN";
    case TOK_ASSIGN:
      return "TOK_ASSIGN";
    case TOK_PLUSPLUS:
      return "TOK_PLUSPLUS";
    case TOK_EQEQ:
      return "TOK_EQEQ";
    case TOK_LT:
      return "TOK_LT";
    case TOK_GT:
      return "TOK_GT";
    case TOK_GTEQ:
      return "TOK_GTEQ";
    case TOK_AND:
      return "TOK_AND";
    case TOK_OR:
      return "TOK_OR";
    case TOK_NOT:
      return "TOK_NOT";
    case TOK_TRUE:
      return "TOK_TRUE";
    case TOK_FALSE:
      return "TOK_FALSE";
    case TOK_WHILE:
      return "TOK_WHILE";
    case TOK_DEF:
      return "TOK_DEF";
    case TOK_RETURN:
      return "TOK_RETURN";
    case TOK_COMMA:
      return "TOK_COMMA";
    case TOK_COLON:
      return "TOK_COLON";
    case TOK_INDENT:
      return "TOK_INDENT";
    case TOK_DEDENT:
      return "TOK_DEDENT";
    case TOK_EOF:
      return "TOK_EOF";
    case TOK_PRINT:
      return "TOK_PRINT";
    case TOK_FOR:
      return "TOK_FOR";
    case TOK_IN:
      return "TOK_IN";
    case TOK_RANGE:
      return "TOK_RANGE";
    case TOK_IF:
      return "TOK_IF";
    case TOK_ELSE:
      return "TOK_ELSE";
    case TOK_ELIF:
      return "TOK_ELIF";
    default:
      return "UNKNOWN";
    }
  }

public:
  ASTParser(Context &ctx, std::vector<Token> t)
      : ctx(ctx), tokens(std::move(t)) {}
  int parse();
  std::unique_ptr<ASTNode> parse_stmt();
  std::unique_ptr<ASTNode> parse_function();
  std::unique_ptr<ASTNode> parse_while();
  std::unique_ptr<ASTNode> parse_for();
  std::unique_ptr<ASTNode> parse_if();
  std::unique_ptr<ASTNode> parse_global();
  std::unique_ptr<ASTNode> parse_expr();
  std::unique_ptr<ASTNode> parse_logic_or();
  std::unique_ptr<ASTNode> parse_logic_and();
  std::unique_ptr<ASTNode> parse_equality();
  std::unique_ptr<ASTNode> parse_comparison();
  std::unique_ptr<ASTNode> parse_term();
  std::unique_ptr<ASTNode> parse_factor();
  std::unique_ptr<ASTNode> parse_unary();
  std::unique_ptr<ASTNode> parse_primary();
  void print_ast();
};
