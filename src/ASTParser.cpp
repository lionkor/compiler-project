#include "ASTParser.h"

#include <lk/Logger.h>

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <variant>

using namespace AST;

std::shared_ptr<Unit> Parser::unit() {
    auto result = std::make_shared<Unit>();
    std::shared_ptr<FunctionDecl> fn;
    for (;;) {
        if (peek().type == Token::Type::EndOfUnit) {
            break;
        }
        if (current().type == Token::Type::UseKeyword) {
            auto use = use_decl();
            if (use) {
                result->use_decls.push_back(use);
            } else {
                break;
            }
        } else {
            fn = function_decl();
            if (fn) {
                result->decls.push_back(fn);
            } else {
                break;
            }
        }
    }
    return result;
}

std::shared_ptr<FunctionDecl> Parser::function_decl() {
    auto result = std::make_shared<FunctionDecl>();
    if (!match({ Token::Type::FnKeyword })) {
        return nullptr;
    }
    result->name = identifier();
    if (!match({ Token::Type::OpeningParentheses })) {
        return nullptr;
    }
    if (!check(Token::Type::ClosingParentheses)) {
        // have arguments
        result->arguments = variable_decl_list();
        if (!result->arguments) {
            return nullptr;
        }
        if (!match({ Token::Type::ClosingParentheses })) {
            return nullptr;
        }
    } else {
        advance();
    }
    if (check(Token::Type::ArrowOperator)) {
        advance();
        result->result = variable_decl();
        if (!result->result) {
            return nullptr;
        }
    }
    result->body = body();
    if (!result->body) {
        return nullptr;
    }
    return result;
}

std::shared_ptr<VariableDecl> Parser::variable_decl() {
    auto result = std::make_shared<VariableDecl>();
    result->type_name = type_name();
    if (!result->type_name) {
        return nullptr;
    }
    result->identifier = identifier();
    if (!result->identifier) {
        return nullptr;
    }
    return result;
}

std::shared_ptr<VariableDeclList> Parser::variable_decl_list() {
    auto result = std::make_shared<VariableDeclList>();
    std::shared_ptr<VariableDecl> decl;
    decl = variable_decl();
    if (decl) {
        result->variables.push_back(decl);
    } else {
        error("variable declaration list is empty");
    }
    for (;;) {
        if (check(Token::Type::Comma)) {
            advance();
            decl = variable_decl();
            if (decl) {
                result->variables.push_back(decl);
            } else {
                return nullptr;
            }
        } else {
            break;
        }
    }
    return result;
}

std::shared_ptr<Body> Parser::body() {
    auto result = std::make_shared<Body>();
    if (!match({ Token::Type::OpeningBrace })) {
        return nullptr;
    }
    result->statements = statements();
    if (!result->statements) {
        return nullptr;
    }
    if (!match({ Token::Type::ClosingBrace })) {
        return nullptr;
    }
    return result;
}

std::shared_ptr<Statement> Parser::statement() {
    auto result = std::make_shared<Statement>();
    if (check(Token::Type::Identifier) && peek().type == Token::Type::OpeningParentheses) {
        result->statement = function_call();
    } else if (check(Token::Type::Typename)) {
        result->statement = variable_decl();
    } else {
        result->statement = assignment();
    }
    if (!result->statement) {
        return nullptr;
    }
    if (!match({ Token::Type::Semicolon })) {
        return nullptr;
    }
    return result;
}

// TODO statements should be (statement)* on body
std::shared_ptr<Statements> Parser::statements() {
    auto result = std::make_shared<Statements>();
    for (;;) {
        if (check(Token::Type::ClosingBrace)) {
            // end of block
            break;
        }
        auto stmt = statement();
        if (stmt) {
            result->statements.push_back(stmt);
        } else {
            break;
        }
    }
    return result;
}

std::shared_ptr<Assignment> Parser::assignment() {
    auto result = std::make_shared<Assignment>();
    result->identifier = identifier();
    if (!result->identifier) {
        return nullptr;
    }
    if (!match({ Token::Type::Equals })) {
        return nullptr;
    }
    result->expression = expression();
    return result;
}

std::shared_ptr<Identifier> Parser::identifier() {
    auto result = std::make_shared<Identifier>();
    if (!check(Token::Type::Identifier)) {
        error_expected(Token::Type::Identifier);
        return nullptr;
    }
    result->name = std::get<std::string>(current().value);
    advance();
    return result;
}

std::shared_ptr<Expression> Parser::expression() {
    auto result = std::make_shared<Expression>();
    result->term = term();
    if (!result->term) {
        return nullptr;
    }
    return result;
}

std::shared_ptr<FunctionCall> Parser::function_call() {
    auto result = std::make_shared<FunctionCall>();
    result->name = identifier();
    if (!result->name) {
        return nullptr;
    }
    if (!match({ Token::Type::OpeningParentheses })) {
        return nullptr;
    }
    bool first_arg = true;
    while (!check(Token::Type::ClosingParentheses)) {
        if (!first_arg && !match({ Token::Type::Comma })) {
            return nullptr;
        }
        if (first_arg) {
            first_arg = false;
        }
        auto next_arg = expression();
        if (!next_arg) {
            error("expected expression for function argument, instead got invalid expression");
            return nullptr;
        }
        result->arguments.push_back(next_arg);
    }
    if (!match({ Token::Type::ClosingParentheses })) {
        return nullptr;
    }
    return result;
}

std::shared_ptr<Term> Parser::term() {
    auto result = std::make_shared<Term>();
    for (;;) {
        std::shared_ptr<Factor> thisfactor = factor();
        if (!thisfactor) {
            return nullptr;
        }
        result->factors.push_back(thisfactor);
        if (check_any_of({ Token::Type::PlusOperator, Token::Type::MinusOperator })) {
            std::string op;
            op += std::get<char>(current().value);
            advance();
            result->operators.push_back(op);
        } else {
            break;
        }
    }
    return result;
}

std::shared_ptr<Factor> Parser::factor() {
    auto result = std::make_shared<Factor>();
    for (;;) {
        auto thisunary = unary();
        if (!thisunary) {
            return nullptr;
        }
        result->unaries.push_back(thisunary);
        if (check_any_of({ Token::Type::MultiplyOperator, Token::Type::DivideOperator })) {
            std::string op;
            op += std::get<char>(current().value);
            advance();
            result->operators.push_back(op);
        } else {
            break;
        }
    }
    return result;
}

std::shared_ptr<Unary> Parser::unary() {
    auto result = std::make_shared<Unary>();
    if (check(Token::Type::MinusOperator)) {
        std::string op;
        op += std::get<char>(current().value);
        advance();
        result->op = op;
        result->unary_or_primary = unary();
    } else {
        result->unary_or_primary = primary();
    }
    if (!result->unary_or_primary) {
        return nullptr;
    }
    return result;
}

std::shared_ptr<Primary> Parser::primary() {
    auto result = std::make_shared<Primary>();
    if (check(Token::Type::NumericLiteral)) {
        result->value = numeric_literal();
    } else if (check(Token::Type::StringLiteral)) {
        result->value = string_literal();
    } else if (check(Token::Type::Identifier)) {
        if (peek().type == Token::Type::OpeningParentheses) {
            result->value = function_call();
        } else {
            result->value = identifier();
        }
    } else {
        result->value = grouped_expression();
    }
    if (!result->value) {
        return nullptr;
    }
    return result;
}

std::shared_ptr<GroupedExpression> Parser::grouped_expression() {
    auto result = std::make_shared<GroupedExpression>();
    if (!match({ Token::Type::OpeningParentheses })) {
        return nullptr;
    }
    result->expression = expression();
    if (!result->expression) {
        return nullptr;
    }
    if (!match({ Token::Type::ClosingParentheses })) {
        return nullptr;
    }
    return result;
}

std::shared_ptr<Typename> Parser::type_name() {
    auto result = std::make_shared<Typename>();
    if (!check(Token::Type::Typename)) {
        error_expected(Token::Type::Typename);
        return nullptr;
    }
    result->name = std::get<std::string>(current().value);
    advance();
    return result;
}

std::shared_ptr<UseDecl> Parser::use_decl() {
    auto result = std::make_shared<UseDecl>();
    if (!match({ Token::Type::UseKeyword, Token::Type::StringLiteral })) {
        return nullptr;
    }
    result->path = std::get<std::string>(previous().value);
    if (!match({ Token::Type::Semicolon })) {
        return nullptr;
    }
    return result;
}

std::shared_ptr<NumericLiteral> Parser::numeric_literal() {
    auto result = std::make_shared<NumericLiteral>();
    if (!match({ Token::Type::NumericLiteral })) {
        return nullptr;
    }
    result->value = std::get<size_t>(previous().value);
    return result;
}

std::shared_ptr<StringLiteral> Parser::string_literal() {
    auto result = std::make_shared<StringLiteral>();
    if (!match({ Token::Type::StringLiteral })) {
        return nullptr;
    }
    result->value = std::get<std::string>(previous().value);
    return result;
}

std::shared_ptr<Node> Parser::literal() {
    if (check(Token::Type::Identifier)) {
        return identifier();
    } else if (check(Token::Type::NumericLiteral)) {
        return numeric_literal();
    } else if (check(Token::Type::StringLiteral)) {
        return string_literal();
    }
    error("expected literal");
    return nullptr;
}

bool Parser::match(std::vector<Token::Type> types) {
    bool result = true;
    for (auto type : types) {
        if (check(type)) {
            advance();
        } else {
            result = false;
            error_expected(type);
            break;
        }
    }
    return result;
}

bool Parser::check(Token::Type type) {
    return current().type == type;
}

bool Parser::check_any_of(std::vector<Token::Type> types) {
    return std::any_of(types.begin(), types.end(), [&](Token::Type type) { return current().type == type; });
}

Token Parser::previous() {
    if (m_i == 0) {
        return Token { Token::Type::StartOfUnit, "", m_tokens[m_i].line };
    } else {
        return m_tokens[m_i - 1];
    }
}

Token Parser::peek() {
    if (m_i + 1 >= m_tokens.size()) {
        return Token { Token::Type::EndOfUnit, "", m_tokens[m_i].line };
    } else {
        return m_tokens[m_i + 1];
    }
}

void Parser::error(const std::string& what) {
    if (m_errors_enabled) {
        ++m_error_count;
        lk::log::error() << "error: line " << current().line << ": " << what << std::endl;
    }
}

void Parser::error_expected(Token::Type expected) {
    std::stringstream ss;
    ss << "expected " << expected << " (between " << previous().type << " and " << peek().type << "), instead got " << current().type;
    error(ss.str());
}

static inline std::string indent(size_t level) {
    std::string result;
    for (size_t i = 0; i < level; ++i) {
        if (i % 2 == 0) {
            result += "| ";
        } else {
            result += " ";
        }
    }
    return result;
}

std::string Statement::to_string(size_t level) {
    return "Statement\n" + indent(level) + statement->to_string(level + 1);
}

std::string Expression::to_string(size_t level) {
    return "Expression\n" + indent(level) + term->to_string(level + 1);
}

std::string Term::to_string(size_t level) {
    std::string res = "Term\n";
    ssize_t k = -1;
    for (size_t i = 0; i < factors.size(); ++i, ++k) {
        if (k >= 0) {
            res += indent(level) + "operator " + operators.at(k) + "\n";
        }
        res += indent(level) + factors.at(i)->to_string(level + 1);
    }
    return res;
}

std::string Factor::to_string(size_t level) {
    std::string res = "Factor\n";
    for (size_t i = 0; i < unaries.size(); ++i) {
        if ((i + 1) % 2 == 0) {
            res += indent(level) + "operator " + operators.at(i - 1) + "\n";
        }
        res += indent(level) + unaries.at(i)->to_string(level + 1);
    }
    return res;
}

std::string Unary::to_string(size_t level) {
    std::string res = "Unary\n";
    if (!empty(op)) {
        res += indent(level) + "operator " + op + "\n";
    }
    res += indent(level) + unary_or_primary->to_string(level + 1);
    return res;
}

std::string Primary::to_string(size_t level) {
    return "Primary\n" + indent(level) + value->to_string(level + 1);
}

std::string GroupedExpression::to_string(size_t level) {
    return "GroupedExpression\n" + indent(level) + expression->to_string(level + 1);
}

std::string Assignment::to_string(size_t level) {
    std::string res = "Assignment\n";
    res += indent(level) + identifier->to_string(level + 1);
    res += indent(level) + expression->to_string(level + 1);
    return res;
}

std::string Statements::to_string(size_t level) {
    std::string res = "Statements\n";
    for (auto& statement : statements) {
        res += indent(level) + statement->to_string(level + 1);
    }
    return res;
}

std::string Body::to_string(size_t level) {
    std::string res = "Body\n";
    res += indent(level) + statements->to_string(level + 1);
    return res;
}

std::string Identifier::to_string(size_t) {
    return "Identifier: " + name + "\n";
}

std::string Typename::to_string(size_t) {
    return "Typename: " + name + "\n";
}

std::string NumericLiteral::to_string(size_t) {
    return "NumericLiteral: " + std::to_string(value) + "\n";
}

std::string StringLiteral::to_string(size_t) {
    return "StringLiteral: \"" + value + "\"" + "\n";
}

std::string VariableDecl::to_string(size_t level) {
    std::string res = "VariableDecl\n";
    res += indent(level) + type_name->to_string(level + 1);
    res += indent(level) + identifier->to_string(level + 1);
    return res;
}

std::string VariableDeclList::to_string(size_t level) {
    std::string res = "VariableDeclList\n";
    for (auto& variable : variables) {
        res += indent(level) + variable->to_string(level + 1);
    }
    return res;
}

std::string FunctionDecl::to_string(size_t level) {
    std::string res = "Function\n";
    res += indent(level) + name->to_string(level + 1);
    if (arguments) {
        res += indent(level) + "Arguments: " + arguments->to_string(level + 1);
    }
    if (result) {
        res += indent(level) + "Result: " + result->to_string(level + 1);
    }
    res += indent(level) + body->to_string(level + 1);
    return res;
}

std::string Unit::to_string(size_t level) {
    std::string res = "Unit\n";
    for (auto& use : use_decls) {
        res += indent(level) + use->to_string(level + 1);
    }
    for (auto& fn : decls) {
        res += indent(level) + fn->to_string(level + 1);
    }
    return res;
}

std::string FunctionCall::to_string(size_t level) {
    std::string res = "FunctionCall\n";
    res += indent(level) + name->to_string(level + 1);
    res += indent(level) + "Arguments:\n";
    for (const auto& arg : arguments) {
        res += indent(level + 1) + arg->to_string(level + 2);
    }
    return res;
}

std::string UseDecl::to_string(size_t) {
    std::string res = "UseDecl: \"" + path + "\"\n";
    return res;
}
