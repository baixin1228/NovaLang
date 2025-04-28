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
    case TOK_STRING:
      return "TOK_STRING";
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
    case TOK_DSLASH:
      return "TOK_DSLASH";
    case TOK_LPAREN:
      return "TOK_LPAREN";
    case TOK_RPAREN:
      return "TOK_RPAREN";
    case TOK_LBRACE:
      return "TOK_LBRACE";
    case TOK_RBRACE:
      return "TOK_RBRACE";
    case TOK_LBRACKET:
      return "TOK_LBRACKET";
    case TOK_RBRACKET:
      return "TOK_RBRACKET";
    case TOK_COLON:
      return "TOK_COLON";
    case TOK_COMMA:
      return "TOK_COMMA";
    case TOK_ASSIGN:
      return "TOK_ASSIGN";
    case TOK_PLUSPLUS:
      return "TOK_PLUSPLUS";
    case TOK_PLUSEQ:
      return "TOK_PLUSEQ";
    case TOK_MINUSEQ:
      return "TOK_MINUSEQ";
    case TOK_STAREQ:
      return "TOK_STAREQ";
    case TOK_SLASHEQ:
      return "TOK_SLASHEQ";
    case TOK_EQEQ:
      return "TOK_EQEQ";
    case TOK_LT:
      return "TOK_LT";
    case TOK_GT:
      return "TOK_GT";
    case TOK_GTEQ:
      return "TOK_GTEQ";
    case TOK_LTEQ:
      return "TOK_LTEQ";
    case TOK_NEQ:
      return "TOK_NEQ";
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
    case TOK_INDENT:
      return "TOK_INDENT";
    case TOK_DEDENT:
      return "TOK_DEDENT";
    case TOK_NEWLINE:
      return "TOK_NEWLINE";
    case TOK_EOF:
      return "TOK_EOF";
    case TOK_GLOBAL:
      return "TOK_GLOBAL";
    case TOK_DOT:
      return "TOK_DOT";
    case TOK_MODULO:
      return "TOK_MODULO";
    case TOK_EXPONENT:
      return "TOK_EXPONENT";
    case TOK_DIVEQ:
      return "TOK_DIVEQ";
    case TOK_CLASS:
      return "TOK_CLASS";
    case TOK_INIT:
      return "TOK_INIT";
    case TOK_INHERIT:
      return "TOK_INHERIT";
    case TOK_SUPER:
      return "TOK_SUPER";
    default:
      return "UNKNOWN";
    }
  }

public:
  ASTParser(Context &ctx, std::vector<Token> t)
      : ctx(ctx), tokens(std::move(t)) {}
  int parse();
  std::shared_ptr<ASTNode> parse_stmt();
  std::shared_ptr<ASTNode> parse_function();
  std::shared_ptr<ASTNode> parse_method(std::string class_name);
  std::shared_ptr<ASTNode> parse_class();
  std::shared_ptr<ASTNode> parse_while();
  std::shared_ptr<ASTNode> parse_for();
  std::shared_ptr<ASTNode> parse_if();
  std::shared_ptr<ASTNode> parse_global();
  std::shared_ptr<ASTNode> parse_expr();
  std::shared_ptr<ASTNode> parse_logic_or();
  std::shared_ptr<ASTNode> parse_logic_and();
  std::shared_ptr<ASTNode> parse_equality();
  std::shared_ptr<ASTNode> parse_comparison();
  std::shared_ptr<ASTNode> parse_term();
  std::shared_ptr<ASTNode> parse_factor();
  std::shared_ptr<ASTNode> parse_unary();
  std::shared_ptr<ASTNode> parse_primary();
  std::shared_ptr<ASTNode> parse_struct_literal();
  std::shared_ptr<ASTNode> parse_dict_literal();
  std::shared_ptr<ASTNode> parse_list_literal();
  std::shared_ptr<ASTNode> parse_call(std::string method_name, std::shared_ptr<ASTNode> last_expr, int line);
  std::shared_ptr<ASTNode> parse_assign_expr(std::vector<std::string>& ids);
  std::shared_ptr<ASTNode> parse_variable_and_field_access(const std::string& id, int ln);
  Token peek_next() const;
  void print_ast();
};
