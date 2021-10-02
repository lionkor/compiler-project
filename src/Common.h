#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

struct Token {
    enum class Type {
        Typename,
        FnKeyword,
        ArrowOperator,
        Identifier,
        OpeningParentheses,
        ClosingParentheses,
        NumericLiteral,
        StringLiteral,
        Comma,
        Equals,
        Semicolon,
        OpeningBrace,
        ClosingBrace,
        PlusOperator,
        MinusOperator,
        MultiplyOperator,
        DivideOperator,
        // special types!
        EndOfUnit,
        StartOfUnit,
    } type;
    std::variant<size_t, char, std::string> value;
    size_t line;
};

static inline std::ostream& operator<<(std::ostream& os, const Token::Type& type) {
    switch (type) {
    case Token::Type::FnKeyword:
        os << "keyword 'fn'";
        break;
    case Token::Type::ArrowOperator:
        os << "operator '->'";
        break;
    case Token::Type::Typename:
        os << "typename";
        break;
    case Token::Type::Equals:
        os << "operator '='";
        break;
    case Token::Type::OpeningBrace:
        os << "opening brace '{'";
        break;
    case Token::Type::ClosingBrace:
        os << "closing brace '}'";
        break;
    case Token::Type::PlusOperator:
        os << "operator '+'";
        break;
    case Token::Type::MinusOperator:
        os << "operator '-'";
        break;
    case Token::Type::MultiplyOperator:
        os << "operator '*'";
        break;
    case Token::Type::DivideOperator:
        os << "operator '/'";
        break;
    case Token::Type::Comma:
        os << "comma ','";
        break;
    case Token::Type::Identifier:
        os << "identifier";
        break;
    case Token::Type::OpeningParentheses:
        os << "opening parentheses '('";
        break;
    case Token::Type::ClosingParentheses:
        os << "closing parentheses ')'";
        break;
    case Token::Type::NumericLiteral:
        os << "numeric literal";
        break;
    case Token::Type::StringLiteral:
        os << "string literal";
        break;
    case Token::Type::Semicolon:
        os << "semicolon ';'";
        break;
    case Token::Type::EndOfUnit:
        os << "end of unit";
        break;
    case Token::Type::StartOfUnit:
        os << "start of unit";
    }
    return os;
}

static inline const std::vector<std::string> typenames = {
    "i64",
    "u64",
    "bool",
    "char",
};
