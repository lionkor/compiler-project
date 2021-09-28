#include <String.h>
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <variant>

const char* test_program = R"(syscall3(1, 0, "hello\n", 6);)";

struct Token {
    enum class Type {
        Identifier,
        OpenParentheses,
        ClosingParentheses,
        NumericLiteral,
        StringLiteral,
        Comma,
        Semicolon,
    } type;
    std::variant<size_t, char, std::string> value;
    size_t line;
};

int main(int, char**) {
    std::string source(test_program);
    std::cout << "test program: \"" << source << "\"\n";
    std::vector<Token> tokens;
    size_t line = 1;
    for (auto iter = source.begin(); iter != source.end() && *iter; ++iter) {
        Token tok;
        tok.line = line;
        if (*iter == ' ' || *iter == '\t') {
            continue;
        } else if (*iter == '\n') {
            ++line;
            continue;
        } else if (std::isalpha(*iter)) { // identifer
            auto end = std::find_if_not(iter, source.end(), [](char c) { return std::isalnum(c); });
            tok.type = Token::Type::Identifier;
            tok.value = std::string(iter, end);
            iter = end - 1;
        } else if (*iter == '(') { // open parens
            tok.type = Token::Type::OpenParentheses;
        } else if (*iter == ')') { // closing parens
            tok.type = Token::Type::ClosingParentheses;
        } else if (std::isdigit(*iter)) { // numeric literal
            auto end = std::find_if_not(iter, source.end(), [](char c) { return std::isdigit(c); });
            auto str = std::string(iter, end);
            size_t value;
            std::from_chars(iter.base(), end.base(), value);
            tok.type = Token::Type::NumericLiteral;
            tok.value = value;
            iter = end - 1;
        } else if (*iter == ',') {
            tok.type = Token::Type::Comma;
        } else if (*iter == ';') {
            tok.type = Token::Type::Semicolon;
        } else if (*iter == '"') {
            auto end = std::find_if(iter + 1, source.end(), [](char c) { return c == '"'; });
            if (end == source.end()) {
                std::cout << line << ": end of file before end of string literal!\n";
                continue;
            }
            tok.type = Token::Type::StringLiteral;
            tok.value = std::string(iter + 1, end - 1);
            iter = end;
        } else {
            std::cout << line << ": error: couldn't parse: " << std::string(&*iter) << "\n";
            continue;
        }
        std::cout << line << ": adding token: " << size_t(tok.type) << "\n";
        tokens.push_back(std::move(tok));
    }
    size_t last_line = 1;
    for (const auto& tok : tokens) {
        if (tok.line > last_line) {
            std::cout << "\n";
            last_line = line;
        }
        switch (tok.type) {
        case Token::Type::Comma:
            std::cout << ",";
            break;
        case Token::Type::Identifier:
            std::cout << "id";
            break;
        case Token::Type::OpenParentheses:
            std::cout << "(";
            break;
        case Token::Type::ClosingParentheses:
            std::cout << ")";
            break;
        case Token::Type::NumericLiteral:
            std::cout << "num";
            break;
        case Token::Type::StringLiteral:
            std::cout << "string";
            break;
        case Token::Type::Semicolon:
            std::cout << ";";
            break;
        }
    }
    std::cout << std::endl;

    /*
    // syntax check
    for (auto iter = tokens.begin(); iter != tokens.end();) {
        auto consume = [&] { ++iter; };
        auto next_is = [&](Token::Type type) -> bool {
            if (iter + 1 >= tokens.end()) {
                return false;
            } else {
                return (iter + 1)->type == type;
            }
        };
        auto expect_consume = [&](std::vector<Token::Type> types) -> bool {
            if (std::none_of(types.begin(), types.end(), [&](Token::Type t) { return t == iter->type; })) {
                std::cout << iter->line << ": token " << size_t(iter->type) << " did not encounter expected types.\n";
                return false;
            }
            consume();
            return true;
        };
    }*/
}
