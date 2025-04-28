#include "ASTParser.h"
#include "ASTNode.h"
#include "Assign.h"
#include "BinOp.h"
#include "BoolLiteral.h"
#include "Call.h"
#include "Context.h"
#include "FloatLiteral.h"
#include "For.h"
#include "Function.h"
#include "If.h"
#include "Increment.h"
#include "IntLiteral.h"
#include "Lexer.h"
#include "Print.h"
#include "Return.h"
#include "UnaryOp.h"
#include "Variable.h"
#include "While.h"
#include "Global.h"
#include "CompoundAssign.h"
#include "StringLiteral.h"
#include "AST/StructLiteral.h"
#include "AST/DictLiteral.h"
#include "AST/ListLiteral.h"
#include "AST/StructFieldAccess.h"
#include "AST/StructFieldAssign.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#define CONSUME_NEW_LINE \
    while (current().type == TOK_NEWLINE) { \
        consume(TOK_NEWLINE, __FILE__, __LINE__); \
    }

int ASTParser::parse() {
    int i = 0;

    while (current().type != TOK_EOF) {
        if (current().type == TOK_NEWLINE || current().type == TOK_DEDENT) {
            consume(current().type, __FILE__, __LINE__);
            continue;
        }
        auto stmt = parse_stmt();
        if (stmt) {
            ctx.add_ast_node(std::move(stmt));
        } else {
            return -1;
        }
    }
    return 0;
}

std::shared_ptr<ASTNode> ASTParser::parse_stmt() {
    CONSUME_NEW_LINE;
    if (current().type == TOK_DEF) {
        return parse_function();
    }
    if (current().type == TOK_CLASS) {
        return parse_class();
    }
    if (current().type == TOK_WHILE) {
        return parse_while();
    }
    if (current().type == TOK_FOR) {
        return parse_for();
    }
    if (current().type == TOK_IF) {
        return parse_if();
    }
    if (current().type == TOK_RETURN) {
        int ln = current().line;
        consume(TOK_RETURN, __FILE__, __LINE__);
        auto value = parse_expr();
        CONSUME_NEW_LINE;
        return std::make_shared<Return>(ctx, std::move(value), ln);
    }
    if (current().type == TOK_PRINT) {
        int ln = current().line;
        consume(TOK_PRINT, __FILE__, __LINE__);
        consume(TOK_LPAREN, __FILE__, __LINE__);
        
        std::vector<std::shared_ptr<ASTNode>> values;
        if (current().type != TOK_RPAREN) {
            values.push_back(parse_expr());
            while (current().type == TOK_COMMA) {
                consume(TOK_COMMA, __FILE__, __LINE__);
                values.push_back(parse_expr());
            }
        }
        
        consume(TOK_RPAREN, __FILE__, __LINE__);
        CONSUME_NEW_LINE;
        return std::make_shared<Print>(ctx, std::move(values), ln);
    }
    if (current().type == TOK_ID) {
        std::string id = current().value;
        int ln = current().line;
        consume(TOK_ID, __FILE__, __LINE__);
        return parse_variable_and_field_access(id, ln);
    }
    if (current().type == TOK_GLOBAL) {
        return parse_global();
    }
    auto expr = parse_expr();
    if (expr) {
      CONSUME_NEW_LINE;
      return expr;
    }
    return nullptr;
}

std::shared_ptr<ASTNode> ASTParser::parse_function() {
    int ln = current().line;
    consume(TOK_DEF, __FILE__, __LINE__);
    std::string name = current().value;
    consume(TOK_ID, __FILE__, __LINE__);
    consume(TOK_LPAREN, __FILE__, __LINE__);
    std::vector<std::string> params;
    while (current().type != TOK_RPAREN) {
        params.push_back(current().value);
        consume(TOK_ID, __FILE__, __LINE__);
        if (current().type == TOK_COMMA) consume(TOK_COMMA, __FILE__, __LINE__);
    }
    consume(TOK_RPAREN, __FILE__, __LINE__);
    consume(TOK_COLON, __FILE__, __LINE__);
    CONSUME_NEW_LINE;
    consume(TOK_INDENT, __FILE__, __LINE__);
    std::vector<std::shared_ptr<ASTNode>> body;
    while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
        auto stmt = parse_stmt();
        if (stmt) body.push_back(std::move(stmt));
    }
    consume(TOK_DEDENT, __FILE__, __LINE__);
    return std::make_shared<Function>(ctx, name, std::move(params),
                                      std::move(body), ln);
}

std::shared_ptr<ASTNode> ASTParser::parse_while() {
    int ln = current().line;
    consume(TOK_WHILE, __FILE__, __LINE__);
    auto condition = parse_expr();
    consume(TOK_COLON, __FILE__, __LINE__);
    CONSUME_NEW_LINE;
    consume(TOK_INDENT, __FILE__, __LINE__);
    std::vector<std::shared_ptr<ASTNode>> body;
    while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
        auto stmt = parse_stmt();
        if (stmt) body.push_back(std::move(stmt));
    }
    consume(TOK_DEDENT, __FILE__, __LINE__);
    return std::make_shared<While>(ctx, std::move(condition), std::move(body),
                                   ln);
}

std::shared_ptr<ASTNode> ASTParser::parse_for() {
    int ln = current().line;
    consume(TOK_FOR, __FILE__, __LINE__);
    auto loop_var = std::make_shared<Variable>(ctx, current().value, current().line);
    consume(TOK_ID, __FILE__, __LINE__);
    consume(TOK_IN, __FILE__, __LINE__);
    consume(TOK_RANGE, __FILE__, __LINE__);
    consume(TOK_LPAREN, __FILE__, __LINE__);
    auto end = parse_expr();
    consume(TOK_RPAREN, __FILE__, __LINE__);
    consume(TOK_COLON, __FILE__, __LINE__);
    CONSUME_NEW_LINE;
    consume(TOK_INDENT, __FILE__, __LINE__);
    std::vector<std::shared_ptr<ASTNode>> body;
    while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
        auto stmt = parse_stmt();
        if (stmt) body.push_back(std::move(stmt));
    }
    consume(TOK_DEDENT, __FILE__, __LINE__);
    return std::make_shared<For>(ctx, loop_var, std::move(end), std::move(body),
                                 ln);
}

std::shared_ptr<ASTNode> ASTParser::parse_if() {
    int ln = current().line;
    consume(TOK_IF, __FILE__, __LINE__);
    auto condition = parse_expr();
    consume(TOK_COLON, __FILE__, __LINE__);
    CONSUME_NEW_LINE;
    consume(TOK_INDENT, __FILE__, __LINE__);
    std::vector<std::shared_ptr<ASTNode>> body;
    while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
        auto stmt = parse_stmt();
        if (stmt) body.push_back(std::move(stmt));
    }
    consume(TOK_DEDENT, __FILE__, __LINE__);
    std::vector<std::pair<std::shared_ptr<ASTNode>, std::vector<std::shared_ptr<ASTNode>>>> elifs;
    while (current().type == TOK_ELIF) {
        int elif_ln = current().line;
        consume(TOK_ELIF, __FILE__, __LINE__);
        auto elif_cond = parse_expr();
        consume(TOK_COLON, __FILE__, __LINE__);
        CONSUME_NEW_LINE;
        consume(TOK_INDENT, __FILE__, __LINE__);
        std::vector<std::shared_ptr<ASTNode>> elif_body;
        while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
            auto stmt = parse_stmt();
            if (stmt) elif_body.push_back(std::move(stmt));
        }
        consume(TOK_DEDENT, __FILE__, __LINE__);
        elifs.emplace_back(std::move(elif_cond), std::move(elif_body));
    }
    std::vector<std::shared_ptr<ASTNode>> else_body;
    if (current().type == TOK_ELSE) {
        consume(TOK_ELSE, __FILE__, __LINE__);
        consume(TOK_COLON, __FILE__, __LINE__);
        CONSUME_NEW_LINE;
        consume(TOK_INDENT, __FILE__, __LINE__);
        while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
            auto stmt = parse_stmt();
            if (stmt) else_body.push_back(std::move(stmt));
        }
        consume(TOK_DEDENT, __FILE__, __LINE__);
    }
    return std::make_shared<If>(ctx, std::move(condition), std::move(body),
                                std::move(elifs), std::move(else_body), ln);
}

std::shared_ptr<ASTNode> ASTParser::parse_expr() {
    return parse_logic_or();
}

std::shared_ptr<ASTNode> ASTParser::parse_logic_or() {
    auto left = parse_logic_and();
    while (current().type == TOK_OR) {
        int ln = current().line;
        std::string op = "or";
        consume(TOK_OR, __FILE__, __LINE__);
        auto right = parse_logic_and();
        left = std::make_shared<BinOp>(ctx, op, std::move(left),
                                       std::move(right), ln);
    }
    return left;
}

std::shared_ptr<ASTNode> ASTParser::parse_logic_and() {
    auto left = parse_equality();
    while (current().type == TOK_AND) {
        int ln = current().line;
        std::string op = "and";
        consume(TOK_AND, __FILE__, __LINE__);
        auto right = parse_equality();
        left = std::make_shared<BinOp>(ctx, op, std::move(left),
                                       std::move(right), ln);
    }
    return left;
}

std::shared_ptr<ASTNode> ASTParser::parse_equality() {
    auto left = parse_comparison();
    while (current().type == TOK_EQEQ || current().type == TOK_NEQ) {
        int ln = current().line;
        std::string op;
        if (current().type == TOK_EQEQ) {
            op = "==";
            consume(TOK_EQEQ, __FILE__, __LINE__);
        } else {
            op = "!=";
            consume(TOK_NEQ, __FILE__, __LINE__);
        }
        auto right = parse_comparison();
        left = std::make_shared<BinOp>(ctx, op, std::move(left),
                                       std::move(right), ln);
    }
    return left;
}

std::shared_ptr<ASTNode> ASTParser::parse_comparison() {
    auto left = parse_term();
    while (current().type == TOK_LT || current().type == TOK_GT || current().type == TOK_GTEQ || current().type == TOK_LTEQ) {
        int ln = current().line;
        std::string op;
        if (current().type == TOK_LT) {
            op = "<";
            consume(TOK_LT, __FILE__, __LINE__);
        } else if (current().type == TOK_GT) {
            op = ">";
            consume(TOK_GT, __FILE__, __LINE__);
        } else if (current().type == TOK_GTEQ) {
            op = ">=";
            consume(TOK_GTEQ, __FILE__, __LINE__);
        } else {
            op = "<=";
            consume(TOK_LTEQ, __FILE__, __LINE__);
        }
        auto right = parse_term();
        left = std::make_shared<BinOp>(ctx, op, std::move(left),
                                       std::move(right), ln);
    }
    return left;
}

std::shared_ptr<ASTNode> ASTParser::parse_term() {
    auto left = parse_factor();
    while (current().type == TOK_PLUS || current().type == TOK_MINUS) {
        int ln = current().line;
        std::string op;
        if (current().type == TOK_PLUS) {
            op = "+";
            consume(TOK_PLUS, __FILE__, __LINE__);
        } else {
            op = "-";
            consume(TOK_MINUS, __FILE__, __LINE__);
        }
        auto right = parse_factor();
        left = std::make_shared<BinOp>(ctx, op, std::move(left),
                                       std::move(right), ln);
    }
    return left;
}

std::shared_ptr<ASTNode> ASTParser::parse_factor() {
    auto left = parse_unary();
    while (current().type == TOK_STAR ||
        current().type == TOK_SLASH ||
        current().type == TOK_MODULO ||
        current().type == TOK_EXPONENT ||
        current().type == TOK_DSLASH) {
        int ln = current().line;
        std::string op;
        if (current().type == TOK_STAR) {
            op = "*";
            consume(TOK_STAR, __FILE__, __LINE__);
        } else if (current().type == TOK_SLASH) {
            op = "/";
            consume(TOK_SLASH, __FILE__, __LINE__);
        } else if (current().type == TOK_DSLASH) {
            op = "//";
            consume(TOK_DSLASH, __FILE__, __LINE__);
        } else if (current().type == TOK_MODULO) {
            op = "%";
            consume(TOK_MODULO, __FILE__, __LINE__);
        } else {
            op = "**";
            consume(TOK_EXPONENT, __FILE__, __LINE__);
        }
        auto right = parse_unary();
        left = std::make_shared<BinOp>(ctx, op, std::move(left),
                                       std::move(right), ln);
    }
    return left;
}

std::shared_ptr<ASTNode> ASTParser::parse_unary() {
    if (current().type == TOK_MINUS) {
        int ln = current().line;
        consume(TOK_MINUS, __FILE__, __LINE__);
        auto expr = parse_unary();
        // 这里为了简化，直接将 -x 转换为 0 - x
        auto zero = std::make_shared<IntLiteral>(ctx, 0, ln);
        return std::make_shared<BinOp>(ctx, "-", std::move(zero), std::move(expr), ln);
    }
    if (current().type == TOK_NOT) {
        int ln = current().line;
        consume(TOK_NOT, __FILE__, __LINE__);
        auto expr = parse_unary();
        return std::make_shared<UnaryOp>(ctx, "not", std::move(expr), ln);
    }
    
    // 解析主要表达式
    auto primary = parse_primary();
    
    // 检查是否是字段访问 - 允许链式访问 (如 obj.field1.field2)
    while (current().type == TOK_DOT) {
        int ln = current().line;
        consume(TOK_DOT, __FILE__, __LINE__);
        
        if (current().type != TOK_ID) {
            ctx.add_error(ErrorHandler::ErrorLevel::SYNTAX, 
                         "结构体字段访问后必须是标识符，得到: " + token_type_to_string(current().type), 
                         current().line, __FILE__, __LINE__);
            return nullptr;
        }
        
        std::string field_name = current().value;
        consume(TOK_ID, __FILE__, __LINE__);
        if (current().type == TOK_LPAREN) {
          primary = parse_call(field_name, primary, ln);
        } else {
          primary = std::make_shared<StructFieldAccess>(ctx, std::move(primary), field_name, ln);
        }
    }
    
    return primary;
}

std::shared_ptr<ASTNode> ASTParser::parse_primary() {
    Token cur = current();
    
    if (cur.type == TOK_INT) {
        consume(TOK_INT, __FILE__, __LINE__);
        return std::make_shared<IntLiteral>(ctx, cur.int_value, cur.line);
    }
    
    if (cur.type == TOK_FLOAT) {
        consume(TOK_FLOAT, __FILE__, __LINE__);
        return std::make_shared<FloatLiteral>(ctx, cur.float_value, cur.line);
    }
    
    if (cur.type == TOK_STRING) {
        consume(TOK_STRING, __FILE__, __LINE__);
        return std::make_shared<StringLiteral>(ctx, cur.string_value, cur.line);
    }
    
    if (cur.type == TOK_TRUE) {
        consume(TOK_TRUE, __FILE__, __LINE__);
        return std::make_shared<BoolLiteral>(ctx, true, cur.line);
    }
    
    if (cur.type == TOK_FALSE) {
        consume(TOK_FALSE, __FILE__, __LINE__);
        return std::make_shared<BoolLiteral>(ctx, false, cur.line);
    }
    
    // 处理 super 关键字
    if (cur.type == TOK_SUPER) {
        int line = cur.line;
        consume(TOK_SUPER, __FILE__, __LINE__);
        
        // 创建一个表示 super 的变量
        auto super_var = std::make_shared<Variable>(ctx, "super", line);
        
        // 检查是否是方法调用 super()
        if (current().type == TOK_LPAREN) {
            // 处理 super() 调用
            consume(TOK_LPAREN, __FILE__, __LINE__);
            
            // 解析可能的参数
            std::vector<std::shared_ptr<ASTNode>> args;
            if (current().type != TOK_RPAREN) {
                do {
                    auto arg = parse_expr();
                    if (!arg) {
                        return nullptr;
                    }
                    args.push_back(arg);
                    
                    if (current().type != TOK_COMMA) {
                        break;
                    }
                    consume(TOK_COMMA, __FILE__, __LINE__);
                } while (true);
            }
            
            consume(TOK_RPAREN, __FILE__, __LINE__);
            
            // 创建一个super()调用
            auto super_call = std::make_shared<Call>(ctx, "super", args, nullptr, line);
            
            // 检查是否有后续的方法调用 super().__init__()
            if (current().type == TOK_DOT) {
                // Use StructFieldAccess instead of AttributeAccess
                consume(TOK_DOT, __FILE__, __LINE__);
                std::string method_name = current().value;
                consume(TOK_ID, __FILE__, __LINE__);
                if (current().type == TOK_LPAREN) {
                    return parse_call(method_name, super_call, line);
                } else {
                    return std::make_shared<StructFieldAccess>(ctx, std::move(super_call), method_name, line);
                }
            }
            
            return super_call;
        }
        
        // 检查是否有后续的属性访问 super.attr
        if (current().type == TOK_DOT) {
            // Use StructFieldAccess instead of AttributeAccess
            consume(TOK_DOT, __FILE__, __LINE__);
            std::string field_name = current().value;
            consume(TOK_ID, __FILE__, __LINE__);
            if (current().type == TOK_LPAREN) {
                return parse_call(field_name, super_var, line);
            } else {
                return std::make_shared<StructFieldAccess>(ctx, std::move(super_var), field_name, line);
            }
        }
        
        return super_var;
    }
    
    // 处理 self 关键字，现在作为普通标识符
    if (cur.type == TOK_ID && cur.value == "self") {
        consume(TOK_ID, __FILE__, __LINE__);
        // 创建一个表示 self 的变量
        auto self_var = std::make_shared<Variable>(ctx, "self", cur.line);
        
        // 检查是否紧跟着有点号，如果有则解析结构体字段访问
        if (current().type == TOK_DOT) {
            consume(TOK_DOT, __FILE__, __LINE__);
            std::string field_name = current().value;
            consume(TOK_ID, __FILE__, __LINE__);
            if (current().type == TOK_LPAREN) {
                return parse_call(field_name, self_var, cur.line);
            } else {
                return std::make_shared<StructFieldAccess>(ctx, std::move(self_var), field_name, cur.line);
            }
        }
        
        return self_var;
    }
    
    if (cur.type == TOK_ID) {
        std::string name = cur.value;
        consume(TOK_ID, __FILE__, __LINE__);
        
        if (current().type == TOK_LPAREN) {
            return parse_call(name, nullptr, cur.line);
        }
        
        return std::make_shared<Variable>(ctx, name, cur.line);
    }
    
    if (cur.type == TOK_LPAREN) {
        consume(TOK_LPAREN, __FILE__, __LINE__);
        auto expr = parse_expr();
        consume(TOK_RPAREN, __FILE__, __LINE__);
        return expr;
    }
    if (cur.type == TOK_LBRACE) {
        // 如果是空大括号，直接报错
        if (pos + 1 < tokens.size() && tokens[pos + 1].type == TOK_RBRACE) {
            ctx.add_error(ErrorHandler::ErrorLevel::SYNTAX, 
                         "不支持空的大括号语法，请指定类型信息", 
                         current().line, __FILE__, __LINE__);
            // 消费tokens以继续解析
            consume(TOK_LBRACE, __FILE__, __LINE__);
            consume(TOK_RBRACE, __FILE__, __LINE__);
            return nullptr;
        }
        
        // 判断是结构体还是字典
        if (pos + 1 < tokens.size() && 
            tokens[pos + 1].type == TOK_ID && 
            pos + 2 < tokens.size() && 
            tokens[pos + 2].type == TOK_ASSIGN) {
            return parse_struct_literal();
        } else if (pos + 1 < tokens.size() && 
                  (tokens[pos + 1].type == TOK_STRING && pos + 2 < tokens.size() && tokens[pos + 2].type == TOK_COLON)) {
            return parse_dict_literal();
        } else {
            throw std::runtime_error("Unexpected token: " + current().value + " line: " + std::to_string(__LINE__) + " file: " + __FILE__);
            return nullptr;
        }
    }
    if (cur.type == TOK_LBRACKET) {
        // 如果是空方括号，直接报错
        if (pos + 1 < tokens.size() && tokens[pos + 1].type == TOK_RBRACKET) {
            ctx.add_error(ErrorHandler::ErrorLevel::SYNTAX, 
                         "不支持空的方括号语法，请指定类型信息", 
                         current().line, __FILE__, __LINE__);
            // 消费tokens以继续解析
            consume(TOK_LBRACKET, __FILE__, __LINE__);
            consume(TOK_RBRACKET, __FILE__, __LINE__);
            return nullptr;
        }
        return parse_list_literal();
    }
    throw std::runtime_error("Unexpected token: " + current().value + " source_line: " + std::to_string(current().line) + " line: " + std::to_string(__LINE__) + " file: " + __FILE__);
    return nullptr;
}

std::shared_ptr<ASTNode> ASTParser::parse_global() {
    int ln = current().line;
    consume(TOK_GLOBAL, __FILE__, __LINE__);
    std::vector<std::string> var_names;
    var_names.push_back(current().value);
    consume(TOK_ID, __FILE__, __LINE__);
    CONSUME_NEW_LINE;
    return std::make_shared<Global>(ctx, std::move(var_names), ln);
}

void ASTParser::print_ast() {
    std::cout << "[AST]\nProgram\n";
    for (const auto& pair : ctx.get_ast()) {
        pair->print(1);
    }
}

Token ASTParser::peek_next() const {
    if (pos + 1 < tokens.size()) {
        return tokens[pos + 1];
    }
    return Token(TOK_EOF, "", 0);
}

// 解析结构体字面量
std::shared_ptr<ASTNode> ASTParser::parse_struct_literal() {
    int ln = current().line;
    consume(TOK_LBRACE, __FILE__, __LINE__);
    std::map<std::string, std::shared_ptr<ASTNode>> fields;
    
    // 不再处理空结构体，因为这已经在parse_primary中处理了
    
    // 解析第一个字段
    if (current().type != TOK_ID) {
        ctx.add_error(ErrorHandler::ErrorLevel::SYNTAX, 
                     "结构体字段名必须是标识符，得到: " + token_type_to_string(current().type), 
                     current().line, __FILE__, __LINE__);
        return nullptr;
    }
    
    std::string field_name = current().value;
    consume(TOK_ID, __FILE__, __LINE__);
    consume(TOK_ASSIGN, __FILE__, __LINE__);
    auto field_value = parse_expr();
    if (!field_value) {
        return nullptr;
    }
    fields[field_name] = std::move(field_value);
    
    // 解析剩余字段
    while (current().type == TOK_COMMA) {
        consume(TOK_COMMA, __FILE__, __LINE__);
        
        // 处理结尾的逗号情况
        if (current().type == TOK_RBRACE) {
            break;
        }
        
        if (current().type != TOK_ID) {
            ctx.add_error(ErrorHandler::ErrorLevel::SYNTAX, 
                         "结构体字段名必须是标识符，得到: " + token_type_to_string(current().type), 
                         current().line, __FILE__, __LINE__);
            return nullptr;
        }
        
        field_name = current().value;
        consume(TOK_ID, __FILE__, __LINE__);
        consume(TOK_ASSIGN, __FILE__, __LINE__);
        field_value = parse_expr();
        if (!field_value) {
            return nullptr;
        }
        fields[field_name] = std::move(field_value);
    }
    
    consume(TOK_RBRACE, __FILE__, __LINE__);
    
    // 创建结构体字面量，对于普通结构体，函数和属性为空
    std::vector<std::shared_ptr<ASTNode>> functions;
    std::map<std::string, std::shared_ptr<ASTNode>> attributes;
    
    // 使用空名称和STRUCT类型
    return std::make_shared<StructLiteral>(
        ctx, 
        "",  // 匿名结构体
        std::move(fields), 
        std::move(functions), 
        std::move(attributes),
        StructType::STRUCT,
        ln
    );
}

// 解析字典字面量
std::shared_ptr<ASTNode> ASTParser::parse_dict_literal() {
    int ln = current().line;
    consume(TOK_LBRACE, __FILE__, __LINE__);
    std::vector<std::pair<std::shared_ptr<ASTNode>, std::shared_ptr<ASTNode>>> items;
    
    // 不再处理空字典，因为这已经在parse_primary中处理了
    
    // 解析第一个键值对
    if (current().type != TOK_STRING) {
        ctx.add_error(ErrorHandler::ErrorLevel::SYNTAX, 
                     "字典键必须是字符串字面量，得到: " + token_type_to_string(current().type), 
                     current().line, __FILE__, __LINE__);
        return nullptr;
    }
    
    Token key_token = current();
    consume(TOK_STRING, __FILE__, __LINE__);
    auto key = std::make_shared<StringLiteral>(ctx, key_token.string_value, key_token.line);
    
    consume(TOK_COLON, __FILE__, __LINE__);
    auto value = parse_expr();
    if (!value) {
        return nullptr;
    }
    items.emplace_back(std::move(key), std::move(value));
    
    // 解析剩余键值对
    while (current().type == TOK_COMMA) {
        consume(TOK_COMMA, __FILE__, __LINE__);
        
        // 处理结尾的逗号情况
        if (current().type == TOK_RBRACE) {
            break;
        }
        
        if (current().type != TOK_STRING) {
            ctx.add_error(ErrorHandler::ErrorLevel::SYNTAX, 
                         "字典键必须是字符串字面量，得到: " + token_type_to_string(current().type), 
                         current().line, __FILE__, __LINE__);
            return nullptr;
        }
        
        key_token = current();
        consume(TOK_STRING, __FILE__, __LINE__);
        key = std::make_shared<StringLiteral>(ctx, key_token.string_value, key_token.line);
        
        consume(TOK_COLON, __FILE__, __LINE__);
        value = parse_expr();
        if (!value) {
            return nullptr;
        }
        items.emplace_back(std::move(key), std::move(value));
    }
    
    consume(TOK_RBRACE, __FILE__, __LINE__);
    return std::make_shared<DictLiteral>(ctx, std::move(items), ln);
}

// 解析列表字面量
std::shared_ptr<ASTNode> ASTParser::parse_list_literal() {
    int ln = current().line;
    consume(TOK_LBRACKET, __FILE__, __LINE__);
    std::vector<std::shared_ptr<ASTNode>> elements;
    
    // 不再处理空列表，因为这已经在parse_primary中处理了
    
    // 解析第一个元素
    auto element = parse_expr();
    if (!element) {
        return nullptr;
    }
    elements.push_back(std::move(element));
    
    // 解析剩余元素
    while (current().type == TOK_COMMA) {
        consume(TOK_COMMA, __FILE__, __LINE__);
        
        // 处理结尾的逗号情况
        if (current().type == TOK_RBRACKET) {
            break;
        }
        
        element = parse_expr();
        if (!element) {
            return nullptr;
        }
        elements.push_back(std::move(element));
    }
    
    consume(TOK_RBRACKET, __FILE__, __LINE__);
    return std::make_shared<ListLiteral>(ctx, std::move(elements), ln);
}

// Helper method to parse assignment expressions
std::shared_ptr<ASTNode> ASTParser::parse_assign_expr(std::vector<std::string>& ids) {
    // Check if we have a chained assignment
    if (current().type == TOK_ID) {
        std::string next_id = current().value;
        consume(TOK_ID, __FILE__, __LINE__);
        
        if (current().type == TOK_ASSIGN) {
            consume(TOK_ASSIGN, __FILE__, __LINE__);
            
            // Add the ID to our list and continue parsing
            ids.push_back(next_id);
            return parse_assign_expr(ids);
        } else {
            // Not a chained assignment, backtrack
            pos--;
        }
    }
    
    // Parse the rightmost expression
    return parse_expr();
}

// Parse method call on an object
std::shared_ptr<ASTNode> ASTParser::parse_call(std::string func_name, std::shared_ptr<ASTNode> last_expr, int line) {
    consume(TOK_LPAREN, __FILE__, __LINE__);
    std::vector<std::shared_ptr<ASTNode>> args;
    while (current().type != TOK_RPAREN) {
        args.push_back(parse_expr());
        if (current().type == TOK_COMMA)
            consume(TOK_COMMA, __FILE__, __LINE__);
    }
    consume(TOK_RPAREN, __FILE__, __LINE__);
    return std::make_shared<Call>(ctx, func_name, std::move(args), last_expr, line);
}

std::shared_ptr<ASTNode> ASTParser::parse_class() {
    int line = current().line;
    consume(TOK_CLASS, __FILE__, __LINE__);
    
    // 获取类名
    std::string class_name = "";
    if (current().type == TOK_ID) {
        class_name = current().value;
        consume(TOK_ID, __FILE__, __LINE__);
    } else {
        ctx.add_error(ErrorHandler::ErrorLevel::SYNTAX, 
                     "类定义必须有名称", 
                     line, __FILE__, __LINE__);
        return nullptr;
    }
    
    // 处理父类（暂时忽略继承关系）
    std::string parent_class = "";
    if (current().type == TOK_LPAREN) {
        consume(TOK_LPAREN, __FILE__, __LINE__);
        if (current().type == TOK_ID) {
            parent_class = current().value;
            consume(TOK_ID, __FILE__, __LINE__);
        }
        consume(TOK_RPAREN, __FILE__, __LINE__);
    }
    
    // 准备存储结构体的不同部分
    std::map<std::string, std::shared_ptr<ASTNode>> fields;      // 实例变量
    std::vector<std::shared_ptr<ASTNode>> functions;             // 函数列表
    std::map<std::string, std::shared_ptr<ASTNode>> attributes;  // 类属性
    
    if (current().type == TOK_COLON) {
        consume(TOK_COLON, __FILE__, __LINE__);
        CONSUME_NEW_LINE;
        consume(TOK_INDENT, __FILE__, __LINE__);
        
        // 解析类体内容
        while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
            if (current().type == TOK_DEF) {
                // 解析函数
                auto func = parse_function();
                if (func) {
                    functions.push_back(func);
                }
            } else {
                // 解析属性
                if (current().type == TOK_ID) {
                    std::string attr_name = current().value;
                    consume(TOK_ID, __FILE__, __LINE__);
                    
                    if (current().type == TOK_ASSIGN) {
                        consume(TOK_ASSIGN, __FILE__, __LINE__);
                        auto value = parse_expr();
                        if (value) {
                            // 这里我们将其视为类属性
                            attributes[attr_name] = value;
                        }
                    } else {
                        // 如果没有初始化值，设置为null
                        attributes[attr_name] = nullptr;
                    }
                }
                CONSUME_NEW_LINE;
            }
        }
        
        consume(TOK_DEDENT, __FILE__, __LINE__);
    }
    
    // 创建一个表示类的StructLiteral节点，使用CLASS类型
    return std::make_shared<StructLiteral>(
        ctx, 
        class_name,
        std::move(fields),       // 实例字段
        std::move(functions),    // 函数列表
        std::move(attributes),   // 类属性
        StructType::CLASS,       // 类型为CLASS
        line
    );
}

std::shared_ptr<ASTNode> ASTParser::parse_variable_and_field_access(const std::string& id, int ln) {
    // Initialize current_expr based on whether it's a function call or variable
    std::shared_ptr<ASTNode> current_expr;
    if (current().type == TOK_LPAREN) {
        current_expr = parse_call(id, nullptr, ln);
    } else {
        current_expr = std::make_shared<Variable>(ctx, id, ln);
    }
    
    // Keep processing while we see dots
    while (current().type == TOK_DOT) {
        consume(TOK_DOT, __FILE__, __LINE__);
        
        if (current().type != TOK_ID) {
            ctx.add_error(ErrorHandler::ErrorLevel::SYNTAX, 
                         "字段访问后必须是标识符，得到: " + token_type_to_string(current().type), 
                         current().line, __FILE__, __LINE__);
            return nullptr;
        }
        
        std::string field_name = current().value;
        consume(TOK_ID, __FILE__, __LINE__);
        
        // Check if this is a method call
        if (current().type == TOK_LPAREN) {
            current_expr = parse_call(field_name, current_expr, ln);
        } else {
            current_expr = std::make_shared<StructFieldAccess>(ctx, std::move(current_expr), field_name, ln);
        }
    }
    
    // Check if this is an assignment
    if (current().type == TOK_ASSIGN) {
        consume(TOK_ASSIGN, __FILE__, __LINE__);
        auto value = parse_expr();
        CONSUME_NEW_LINE;
        
        // If the current_expr is a StructFieldAccess, we need to create a StructFieldAssign
        if (auto field_access = std::dynamic_pointer_cast<StructFieldAccess>(current_expr)) {
            return std::make_shared<StructFieldAssign>(
                ctx, 
                field_access->field_name, 
                field_access->struct_expr,
                std::move(value), 
                ln);
        } else {
            // If it's a simple variable, create a regular assignment
            if (auto var = std::dynamic_pointer_cast<Variable>(current_expr)) {
                return std::make_shared<Assign>(ctx, var->name, std::move(value), ln);
            }
        }
    }
    
    // Handle compound assignments
    if (current().type == TOK_PLUSEQ || current().type == TOK_MINUSEQ || 
        current().type == TOK_STAREQ || current().type == TOK_SLASHEQ) {
        std::string op;
        switch (current().type) {
            case TOK_PLUSEQ: op = "+="; break;
            case TOK_MINUSEQ: op = "-="; break;
            case TOK_STAREQ: op = "*="; break;
            case TOK_SLASHEQ: op = "/="; break;
            default: break;
        }
        consume(current().type, __FILE__, __LINE__);
        auto value = parse_expr();
        CONSUME_NEW_LINE;
        
        // Only allow compound assignments on simple variables for now
        if (auto var = std::dynamic_pointer_cast<Variable>(current_expr)) {
            return std::make_shared<CompoundAssign>(ctx, var->name, op, std::move(value), ln);
        } else {
            ctx.add_error(ErrorHandler::ErrorLevel::SYNTAX,
                         "复合赋值运算符只能用于简单变量",
                         ln, __FILE__, __LINE__);
            return nullptr;
        }
    }
    
    // Handle increment
    if (current().type == TOK_PLUSPLUS) {
        consume(TOK_PLUSPLUS, __FILE__, __LINE__);
        CONSUME_NEW_LINE;
        
        // Only allow increment on simple variables for now
        if (auto var = std::dynamic_pointer_cast<Variable>(current_expr)) {
            return std::make_shared<Increment>(ctx, var->name, ln);
        } else {
            ctx.add_error(ErrorHandler::ErrorLevel::SYNTAX,
                         "自增运算符只能用于简单变量",
                         ln, __FILE__, __LINE__);
            return nullptr;
        }
    }
    
    return current_expr;
}

