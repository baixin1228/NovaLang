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
#include <iostream>
#include <memory>
#include <string>
#include <vector>

int ASTParser::parse() {
    int i = 0;

    while (current().type != TOK_EOF) {
        if (current().type == TOK_NEWLINE || current().type == TOK_DEDENT) {
            consume(current().type, __FILE__, __LINE__);
            continue;
        }
        auto stmt = parse_stmt();
        if (stmt) {
            if (const auto* func = dynamic_cast<const Function*>(&*stmt)) {
              ctx.set_funcline(func->name, ctx.get_ast().size());
              std::cout << func->name << " added to symbols:" << ctx.get_ast().size()
                        << std::endl;
            }
            ctx.add_ast_node(std::move(stmt));
        } else {
            return -1;
        }
    }
    return 0;
}

std::shared_ptr<ASTNode> ASTParser::parse_stmt() {
    while (current().type == TOK_NEWLINE) {
        consume(TOK_NEWLINE, __FILE__, __LINE__);
    }
    if (current().type == TOK_DEF) {
        return parse_function();
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
        while (current().type == TOK_NEWLINE) {
            consume(TOK_NEWLINE, __FILE__, __LINE__);
        }
        return std::make_unique<Return>(ctx, std::move(value), ln);
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
        while (current().type == TOK_NEWLINE) {
            consume(TOK_NEWLINE, __FILE__, __LINE__);
        }
        return std::make_unique<Print>(ctx, std::move(values), ln);
    }
    if (current().type == TOK_ID) {
        std::string id = current().value;
        int ln = current().line;
        consume(TOK_ID, __FILE__, __LINE__);
        if (current().type == TOK_ASSIGN) {
            consume(TOK_ASSIGN, __FILE__, __LINE__);
            auto value = parse_expr();
            while (current().type == TOK_NEWLINE) {
                consume(TOK_NEWLINE, __FILE__, __LINE__);
            }
            std::cout << id << " = " << (value == nullptr) << "\n" << std::endl;
            return std::make_unique<Assign>(ctx, id, std::move(value), ln);
        }
        if (current().type == TOK_PLUSEQ) {
            consume(TOK_PLUSEQ, __FILE__, __LINE__);
            auto value = parse_expr();
            while (current().type == TOK_NEWLINE) {
                consume(TOK_NEWLINE, __FILE__, __LINE__);
            }
            return std::make_unique<CompoundAssign>(ctx, id, "+=", std::move(value), ln);
        }
        if (current().type == TOK_MINUSEQ) {
            consume(TOK_MINUSEQ, __FILE__, __LINE__);
            auto value = parse_expr();
            while (current().type == TOK_NEWLINE) {
                consume(TOK_NEWLINE, __FILE__, __LINE__);
            }
            return std::make_unique<CompoundAssign>(ctx, id, "-=", std::move(value), ln);
        }
        if (current().type == TOK_STAREQ) {
            consume(TOK_STAREQ, __FILE__, __LINE__);
            auto value = parse_expr();
            while (current().type == TOK_NEWLINE) {
                consume(TOK_NEWLINE, __FILE__, __LINE__);
            }
            return std::make_unique<CompoundAssign>(ctx, id, "*=", std::move(value), ln);
        }
        if (current().type == TOK_SLASHEQ) {
            consume(TOK_SLASHEQ, __FILE__, __LINE__);
            auto value = parse_expr();
            while (current().type == TOK_NEWLINE) {
                consume(TOK_NEWLINE, __FILE__, __LINE__);
            }
            return std::make_unique<CompoundAssign>(ctx, id, "/=", std::move(value), ln);
        }
        if (current().type == TOK_PLUSPLUS) {
            consume(TOK_PLUSPLUS, __FILE__, __LINE__);
            while (current().type == TOK_NEWLINE) {
                consume(TOK_NEWLINE, __FILE__, __LINE__);
            }
            return std::make_unique<Increment>(ctx, id, ln);
        }
        pos--;
    }
    if (current().type == TOK_GLOBAL) {
        return parse_global();
    }
    auto expr = parse_expr();
    if (expr) {
        while (current().type == TOK_NEWLINE) {
            consume(TOK_NEWLINE, __FILE__, __LINE__);
        }
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
    while (current().type == TOK_NEWLINE) {
        consume(TOK_NEWLINE, __FILE__, __LINE__);
    }
    consume(TOK_INDENT, __FILE__, __LINE__);
    std::vector<std::shared_ptr<ASTNode>> body;
    while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
        auto stmt = parse_stmt();
        if (stmt) body.push_back(std::move(stmt));
    }
    consume(TOK_DEDENT, __FILE__, __LINE__);
    return std::make_unique<Function>(ctx, name, std::move(params),
                                      std::move(body), ln);
}

std::shared_ptr<ASTNode> ASTParser::parse_while() {
    int ln = current().line;
    consume(TOK_WHILE, __FILE__, __LINE__);
    auto condition = parse_expr();
    consume(TOK_COLON, __FILE__, __LINE__);
    while (current().type == TOK_NEWLINE) {
        consume(TOK_NEWLINE, __FILE__, __LINE__);
    }
    consume(TOK_INDENT, __FILE__, __LINE__);
    std::vector<std::shared_ptr<ASTNode>> body;
    while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
        auto stmt = parse_stmt();
        if (stmt) body.push_back(std::move(stmt));
    }
    consume(TOK_DEDENT, __FILE__, __LINE__);
    return std::make_unique<While>(ctx, std::move(condition), std::move(body),
                                   ln);
}

std::shared_ptr<ASTNode> ASTParser::parse_for() {
    int ln = current().line;
    consume(TOK_FOR, __FILE__, __LINE__);
    std::string iterator = current().value;
    consume(TOK_ID, __FILE__, __LINE__);
    consume(TOK_IN, __FILE__, __LINE__);
    consume(TOK_RANGE, __FILE__, __LINE__);
    consume(TOK_LPAREN, __FILE__, __LINE__);
    auto end = parse_expr();
    consume(TOK_RPAREN, __FILE__, __LINE__);
    consume(TOK_COLON, __FILE__, __LINE__);
    while (current().type == TOK_NEWLINE) {
        consume(TOK_NEWLINE, __FILE__, __LINE__);
    }
    consume(TOK_INDENT, __FILE__, __LINE__);
    std::vector<std::shared_ptr<ASTNode>> body;
    while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
        auto stmt = parse_stmt();
        if (stmt) body.push_back(std::move(stmt));
    }
    consume(TOK_DEDENT, __FILE__, __LINE__);
    return std::make_unique<For>(ctx, iterator, std::move(end), std::move(body),
                                 ln);
}

std::shared_ptr<ASTNode> ASTParser::parse_if() {
    int ln = current().line;
    consume(TOK_IF, __FILE__, __LINE__);
    auto condition = parse_expr();
    consume(TOK_COLON, __FILE__, __LINE__);
    while (current().type == TOK_NEWLINE) {
        consume(TOK_NEWLINE, __FILE__, __LINE__);
    }
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
        consume(TOK_NEWLINE, __FILE__, __LINE__);
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
        consume(TOK_NEWLINE, __FILE__, __LINE__);
        consume(TOK_INDENT, __FILE__, __LINE__);
        while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
            auto stmt = parse_stmt();
            if (stmt) else_body.push_back(std::move(stmt));
        }
        consume(TOK_DEDENT, __FILE__, __LINE__);
    }
    return std::make_unique<If>(ctx, std::move(condition), std::move(body),
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
        left = std::make_unique<BinOp>(ctx, op, std::move(left),
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
        left = std::make_unique<BinOp>(ctx, op, std::move(left),
                                       std::move(right), ln);
    }
    return left;
}

std::shared_ptr<ASTNode> ASTParser::parse_equality() {
    auto left = parse_comparison();
    while (current().type == TOK_EQEQ) {
        int ln = current().line;
        std::string op = "==";
        consume(TOK_EQEQ, __FILE__, __LINE__);
        auto right = parse_comparison();
        left = std::make_unique<BinOp>(ctx, op, std::move(left),
                                       std::move(right), ln);
    }
    return left;
}

std::shared_ptr<ASTNode> ASTParser::parse_comparison() {
    auto left = parse_term();
    while (current().type == TOK_LT || current().type == TOK_GT || current().type == TOK_GTEQ) {
        int ln = current().line;
        std::string op;
        if (current().type == TOK_LT) {
            op = "<";
            consume(TOK_LT, __FILE__, __LINE__);
        } else if (current().type == TOK_GT) {
            op = ">";
            consume(TOK_GT, __FILE__, __LINE__);
        } else {
            op = ">=";
            consume(TOK_GTEQ, __FILE__, __LINE__);
        }
        auto right = parse_term();
        left = std::make_unique<BinOp>(ctx, op, std::move(left),
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
        left = std::make_unique<BinOp>(ctx, op, std::move(left),
                                       std::move(right), ln);
    }
    return left;
}

std::shared_ptr<ASTNode> ASTParser::parse_factor() {
    auto left = parse_unary();
    while (current().type == TOK_STAR || current().type == TOK_SLASH) {
        int ln = current().line;
        std::string op;
        if (current().type == TOK_STAR) {
            op = "*";
            consume(TOK_STAR, __FILE__, __LINE__);
        } else {
            op = "/";
            consume(TOK_SLASH, __FILE__, __LINE__);
        }
        auto right = parse_unary();
        left = std::make_unique<BinOp>(ctx, op, std::move(left),
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
        auto zero = std::make_unique<IntLiteral>(ctx, 0, ln);
        return std::make_unique<BinOp>(ctx, "-", std::move(zero), std::move(expr), ln);
    }
    if (current().type == TOK_NOT) {
        int ln = current().line;
        consume(TOK_NOT, __FILE__, __LINE__);
        auto expr = parse_unary();
        return std::make_unique<UnaryOp>(ctx, "not", std::move(expr), ln);
    }
    
    // 解析主要表达式
    auto primary = parse_primary();
    
    // 检查是否是字段访问
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
        
        primary = std::make_unique<StructFieldAccess>(ctx, std::move(primary), field_name, ln);
    }
    
    return primary;
}

std::shared_ptr<ASTNode> ASTParser::parse_primary() {
    if (current().type == TOK_STRING) {
        std::string value = current().string_value;
        int ln = current().line;
        consume(TOK_STRING, __FILE__, __LINE__);
        return std::make_unique<StringLiteral>(ctx, value, ln);
    }
    if (current().type == TOK_INT) {
        int value = current().int_value;
        int ln = current().line;
        consume(TOK_INT, __FILE__, __LINE__);
        return std::make_unique<IntLiteral>(ctx, value, ln);
    }
    if (current().type == TOK_FLOAT) {
        double value = current().float_value;
        int ln = current().line;
        consume(TOK_FLOAT, __FILE__, __LINE__);
        return std::make_unique<FloatLiteral>(ctx, value, ln);
    }
    if (current().type == TOK_TRUE) {
        int ln = current().line;
        consume(TOK_TRUE, __FILE__, __LINE__);
        return std::make_unique<BoolLiteral>(ctx, true, ln);
    }
    if (current().type == TOK_FALSE) {
        int ln = current().line;
        consume(TOK_FALSE, __FILE__, __LINE__);
        return std::make_unique<BoolLiteral>(ctx, false, ln);
    }
    if (current().type == TOK_ID) {
        std::string name = current().value;
        int ln = current().line;
        consume(TOK_ID, __FILE__, __LINE__);
        if (current().type == TOK_LPAREN) {
            consume(TOK_LPAREN, __FILE__, __LINE__);
            std::vector<std::shared_ptr<ASTNode>> args;
            while (current().type != TOK_RPAREN) {
                args.push_back(parse_expr());
                if (current().type == TOK_COMMA) consume(TOK_COMMA, __FILE__, __LINE__);
            }
            consume(TOK_RPAREN, __FILE__, __LINE__);
            return std::make_unique<Call>(ctx, name, std::move(args), ln);
        }
        return std::make_unique<Variable>(ctx, name, ln);
    }
    if (current().type == TOK_LPAREN) {
        consume(TOK_LPAREN, __FILE__, __LINE__);
        auto expr = parse_expr();
        consume(TOK_RPAREN, __FILE__, __LINE__);
        return expr;
    }
    if (current().type == TOK_LBRACE) {
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
    if (current().type == TOK_LBRACKET) {
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
    throw std::runtime_error("Unexpected token: " + current().value + " line: " + std::to_string(__LINE__) + " file: " + __FILE__);
    return nullptr;
}

std::shared_ptr<ASTNode> ASTParser::parse_global() {
    int ln = current().line;
    consume(TOK_GLOBAL, __FILE__, __LINE__);
    std::vector<std::string> var_names;
    var_names.push_back(current().value);
    consume(TOK_ID, __FILE__, __LINE__);
    while (current().type == TOK_NEWLINE) {
        consume(TOK_NEWLINE, __FILE__, __LINE__);
    }
    return std::make_unique<Global>(ctx, std::move(var_names), ln);
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
    return std::make_unique<StructLiteral>(ctx, std::move(fields), ln);
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
    auto key = std::make_unique<StringLiteral>(ctx, key_token.string_value, key_token.line);
    
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
        key = std::make_unique<StringLiteral>(ctx, key_token.string_value, key_token.line);
        
        consume(TOK_COLON, __FILE__, __LINE__);
        value = parse_expr();
        if (!value) {
            return nullptr;
        }
        items.emplace_back(std::move(key), std::move(value));
    }
    
    consume(TOK_RBRACE, __FILE__, __LINE__);
    return std::make_unique<DictLiteral>(ctx, std::move(items), ln);
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
    return std::make_unique<ListLiteral>(ctx, std::move(elements), ln);
}
