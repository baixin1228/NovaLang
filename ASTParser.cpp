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
#include <iostream>
#include <memory>
#include <string>
#include <vector>

int ASTParser::parse() {
    int i = 0;

    while (current().type != TOK_EOF) {
        if (current().type == TOK_NEWLINE || current().type == TOK_DEDENT) {
            consume(current().type);
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

std::unique_ptr<ASTNode> ASTParser::parse_stmt() {
    while (current().type == TOK_NEWLINE) {
        consume(TOK_NEWLINE);
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
        consume(TOK_RETURN);
        auto value = parse_expr();
        while (current().type == TOK_NEWLINE) {
            consume(TOK_NEWLINE);
        }
        return std::make_unique<Return>(ctx, std::move(value), ln);
    }
    if (current().type == TOK_PRINT) {
        int ln = current().line;
        consume(TOK_PRINT);
        auto value = parse_expr();
        while (current().type == TOK_NEWLINE) {
            consume(TOK_NEWLINE);
        }
        return std::make_unique<Print>(ctx, std::move(value), ln);
    }
    if (current().type == TOK_ID) {
        std::string id = current().value;
        int ln = current().line;
        consume(TOK_ID);
        if (current().type == TOK_ASSIGN) {
            consume(TOK_ASSIGN);
            auto value = parse_expr();
            while (current().type == TOK_NEWLINE) {
                consume(TOK_NEWLINE);
            }
            return std::make_unique<Assign>(ctx, id, std::move(value), ln);
        }
        if (current().type == TOK_PLUSPLUS) {
            consume(TOK_PLUSPLUS);
            while (current().type == TOK_NEWLINE) {
                consume(TOK_NEWLINE);
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
            consume(TOK_NEWLINE);
        }
        return expr;
    }
    return nullptr;
}

std::unique_ptr<ASTNode> ASTParser::parse_function() {
    int ln = current().line;
    consume(TOK_DEF);
    std::string name = current().value;
    consume(TOK_ID);
    consume(TOK_LPAREN);
    std::vector<std::string> params;
    while (current().type != TOK_RPAREN) {
        params.push_back(current().value);
        consume(TOK_ID);
        if (current().type == TOK_COMMA) consume(TOK_COMMA);
    }
    consume(TOK_RPAREN);
    consume(TOK_COLON);
    while (current().type == TOK_NEWLINE) {
        consume(TOK_NEWLINE);
    }
    consume(TOK_INDENT);
    std::vector<std::unique_ptr<ASTNode>> body;
    while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
        auto stmt = parse_stmt();
        if (stmt) body.push_back(std::move(stmt));
    }
    consume(TOK_DEDENT);
    return std::make_unique<Function>(ctx, name, std::move(params),
                                      std::move(body), ln);
}

std::unique_ptr<ASTNode> ASTParser::parse_while() {
    int ln = current().line;
    consume(TOK_WHILE);
    auto condition = parse_expr();
    consume(TOK_COLON);
    while (current().type == TOK_NEWLINE) {
        consume(TOK_NEWLINE);
    }
    consume(TOK_INDENT);
    std::vector<std::unique_ptr<ASTNode>> body;
    while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
        auto stmt = parse_stmt();
        if (stmt) body.push_back(std::move(stmt));
    }
    consume(TOK_DEDENT);
    return std::make_unique<While>(ctx, std::move(condition), std::move(body),
                                   ln);
}

std::unique_ptr<ASTNode> ASTParser::parse_for() {
    int ln = current().line;
    consume(TOK_FOR);
    std::string iterator = current().value;
    consume(TOK_ID);
    consume(TOK_IN);
    consume(TOK_RANGE);
    consume(TOK_LPAREN);
    auto end = parse_expr();
    consume(TOK_RPAREN);
    consume(TOK_COLON);
    while (current().type == TOK_NEWLINE) {
        consume(TOK_NEWLINE);
    }
    consume(TOK_INDENT);
    std::vector<std::unique_ptr<ASTNode>> body;
    while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
        auto stmt = parse_stmt();
        if (stmt) body.push_back(std::move(stmt));
    }
    consume(TOK_DEDENT);
    return std::make_unique<For>(ctx, iterator, std::move(end), std::move(body),
                                 ln);
}

std::unique_ptr<ASTNode> ASTParser::parse_if() {
    int ln = current().line;
    consume(TOK_IF);
    auto condition = parse_expr();
    consume(TOK_COLON);
    while (current().type == TOK_NEWLINE) {
        consume(TOK_NEWLINE);
    }
    consume(TOK_INDENT);
    std::vector<std::unique_ptr<ASTNode>> body;
    while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
        auto stmt = parse_stmt();
        if (stmt) body.push_back(std::move(stmt));
    }
    consume(TOK_DEDENT);
    std::vector<std::pair<std::unique_ptr<ASTNode>, std::vector<std::unique_ptr<ASTNode>>>> elifs;
    while (current().type == TOK_ELIF) {
        int elif_ln = current().line;
        consume(TOK_ELIF);
        auto elif_cond = parse_expr();
        consume(TOK_COLON);
        consume(TOK_NEWLINE);
        consume(TOK_INDENT);
        std::vector<std::unique_ptr<ASTNode>> elif_body;
        while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
            auto stmt = parse_stmt();
            if (stmt) elif_body.push_back(std::move(stmt));
        }
        consume(TOK_DEDENT);
        elifs.emplace_back(std::move(elif_cond), std::move(elif_body));
    }
    std::vector<std::unique_ptr<ASTNode>> else_body;
    if (current().type == TOK_ELSE) {
        consume(TOK_ELSE);
        consume(TOK_COLON);
        consume(TOK_NEWLINE);
        consume(TOK_INDENT);
        while (current().type != TOK_DEDENT && current().type != TOK_EOF) {
            auto stmt = parse_stmt();
            if (stmt) else_body.push_back(std::move(stmt));
        }
        consume(TOK_DEDENT);
    }
    return std::make_unique<If>(ctx, std::move(condition), std::move(body),
                                std::move(elifs), std::move(else_body), ln);
}

std::unique_ptr<ASTNode> ASTParser::parse_expr() {
    return parse_logic_or();
}

std::unique_ptr<ASTNode> ASTParser::parse_logic_or() {
    auto left = parse_logic_and();
    while (current().type == TOK_OR) {
        int ln = current().line;
        std::string op = "or";
        consume(TOK_OR);
        auto right = parse_logic_and();
        left = std::make_unique<BinOp>(ctx, op, std::move(left),
                                       std::move(right), ln);
    }
    return left;
}

std::unique_ptr<ASTNode> ASTParser::parse_logic_and() {
    auto left = parse_equality();
    while (current().type == TOK_AND) {
        int ln = current().line;
        std::string op = "and";
        consume(TOK_AND);
        auto right = parse_equality();
        left = std::make_unique<BinOp>(ctx, op, std::move(left),
                                       std::move(right), ln);
    }
    return left;
}

std::unique_ptr<ASTNode> ASTParser::parse_equality() {
    auto left = parse_comparison();
    while (current().type == TOK_EQEQ) {
        int ln = current().line;
        std::string op = "==";
        consume(TOK_EQEQ);
        auto right = parse_comparison();
        left = std::make_unique<BinOp>(ctx, op, std::move(left),
                                       std::move(right), ln);
    }
    return left;
}

std::unique_ptr<ASTNode> ASTParser::parse_comparison() {
    auto left = parse_term();
    while (current().type == TOK_LT || current().type == TOK_GT || current().type == TOK_GTEQ) {
        int ln = current().line;
        std::string op;
        if (current().type == TOK_LT) {
            op = "<";
            consume(TOK_LT);
        } else if (current().type == TOK_GT) {
            op = ">";
            consume(TOK_GT);
        } else {
            op = ">=";
            consume(TOK_GTEQ);
        }
        auto right = parse_term();
        left = std::make_unique<BinOp>(ctx, op, std::move(left),
                                       std::move(right), ln);
    }
    return left;
}

std::unique_ptr<ASTNode> ASTParser::parse_term() {
    auto left = parse_factor();
    while (current().type == TOK_PLUS || current().type == TOK_MINUS) {
        int ln = current().line;
        std::string op;
        if (current().type == TOK_PLUS) {
            op = "+";
            consume(TOK_PLUS);
        } else {
            op = "-";
            consume(TOK_MINUS);
        }
        auto right = parse_factor();
        left = std::make_unique<BinOp>(ctx, op, std::move(left),
                                       std::move(right), ln);
    }
    return left;
}

std::unique_ptr<ASTNode> ASTParser::parse_factor() {
    auto left = parse_unary();
    while (current().type == TOK_STAR || current().type == TOK_SLASH) {
        int ln = current().line;
        std::string op;
        if (current().type == TOK_STAR) {
            op = "*";
            consume(TOK_STAR);
        } else {
            op = "/";
            consume(TOK_SLASH);
        }
        auto right = parse_unary();
        left = std::make_unique<BinOp>(ctx, op, std::move(left),
                                       std::move(right), ln);
    }
    return left;
}

std::unique_ptr<ASTNode> ASTParser::parse_unary() {
    if (current().type == TOK_NOT) {
        int ln = current().line;
        consume(TOK_NOT);
        auto expr = parse_unary();
        return std::make_unique<UnaryOp>(ctx, "not", std::move(expr), ln);
    }
    return parse_primary();
}

std::unique_ptr<ASTNode> ASTParser::parse_primary() {
    if (current().type == TOK_INT) {
        int value = current().int_value;
        int ln = current().line;
        consume(TOK_INT);
        return std::make_unique<IntLiteral>(ctx, value, ln);
    }
    if (current().type == TOK_FLOAT) {
        double value = current().float_value;
        int ln = current().line;
        consume(TOK_FLOAT);
        return std::make_unique<FloatLiteral>(ctx, value, ln);
    }
    if (current().type == TOK_TRUE) {
        int ln = current().line;
        consume(TOK_TRUE);
        return std::make_unique<BoolLiteral>(ctx, true, ln);
    }
    if (current().type == TOK_FALSE) {
        int ln = current().line;
        consume(TOK_FALSE);
        return std::make_unique<BoolLiteral>(ctx, false, ln);
    }
    if (current().type == TOK_ID) {
        std::string name = current().value;
        int ln = current().line;
        consume(TOK_ID);
        if (current().type == TOK_LPAREN) {
            consume(TOK_LPAREN);
            std::vector<std::unique_ptr<ASTNode>> args;
            while (current().type != TOK_RPAREN) {
                args.push_back(parse_expr());
                if (current().type == TOK_COMMA) consume(TOK_COMMA);
            }
            consume(TOK_RPAREN);
            return std::make_unique<Call>(ctx, name, std::move(args), ln);
        }
        return std::make_unique<Variable>(ctx, name, ln);
    }
    if (current().type == TOK_LPAREN) {
        consume(TOK_LPAREN);
        auto expr = parse_expr();
        consume(TOK_RPAREN);
        return expr;
    }
    throw std::runtime_error("Unexpected token: " + current().value + " line: " + std::to_string(__LINE__) + " file: " + __FILE__);
    return nullptr;
}

std::unique_ptr<ASTNode> ASTParser::parse_global() {
    int ln = current().line;
    consume(TOK_GLOBAL);
    std::vector<std::string> var_names;
    var_names.push_back(current().value);
    consume(TOK_ID);
    while (current().type == TOK_NEWLINE) {
        consume(TOK_NEWLINE);
    }
    return std::make_unique<Global>(ctx, std::move(var_names), ln);
}

void ASTParser::print_ast() {
    std::cout << "[AST]\nProgram\n";
    for (const auto& pair : ctx.get_ast()) {
        pair->print(1);
    }
}
