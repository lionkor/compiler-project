#include <String.h>
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iostream>
#include <variant>

const char* test_program = R"(
fn add(i64 a, i64 b) -> i64 c {
    c = a + b;
}

fn main() -> i64 ret {
    ret = add(1, 2);
}
)";

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

std::ostream& operator<<(std::ostream& os, const Token::Type& type) {
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

std::vector<std::string> typenames = {
    "i64",
    "u64",
    "bool",
    "char",
};

namespace AST {

struct Expr {
};

struct VariableDecl : public Expr {
};

struct VariableDeclList : public Expr {
    std::vector<std::shared_ptr<VariableDecl>> variables;
};

struct Statement : public Expr {
    std::shared_ptr<Expr> statement;
};

struct Statements : public Expr {
    std::vector<std::shared_ptr<Statement>> statements;
};

struct Body : public Expr {
    std::shared_ptr<Statements> statements;
};

struct FunctionDecl : public Expr {
    std::shared_ptr<VariableDeclList> arguments;
    std::shared_ptr<VariableDecl> result;
    std::shared_ptr<Body> body;
};

struct Unit : public Expr {
    std::vector<std::shared_ptr<FunctionDecl>> decls;
};

class Parser {
public:
    Parser(const std::vector<Token>& tokens)
        : m_tokens(tokens) { }
    std::shared_ptr<Unit> unit();
    std::shared_ptr<FunctionDecl> function_decl();
    std::shared_ptr<VariableDecl> variable_decl();
    std::shared_ptr<VariableDeclList> variable_decl_list();
    std::shared_ptr<Body> body();
    std::shared_ptr<Statement> statement();
    std::shared_ptr<Statements> statements();

private:
    bool match(std::vector<Token::Type>);
    bool check(Token::Type);
    void advance() { ++m_i; }
    Token previous();
    Token peek();
    const Token& current() { return m_tokens[m_i]; }
    size_t m_i { 0 };
    std::vector<Token> m_tokens;
};

std::shared_ptr<Unit> Parser::unit() {
    auto result = std::make_shared<Unit>();
    std::shared_ptr<FunctionDecl> fn;
    for (;;) {
        fn = function_decl();
        if (fn) {

            result->decls.push_back(fn);
        } else {
            break;
        }
    }
    return result;
}

std::shared_ptr<FunctionDecl> Parser::function_decl() {
    auto result = std::make_shared<FunctionDecl>();
    if (!match({ Token::Type::FnKeyword, Token::Type::Identifier, Token::Type::OpeningParentheses })) {
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
        result->result = variable_decl();
        if (!result->result) {
            return nullptr;
        }
    } else {
        advance();
    }
    result->body = body();
    if (!result->body) {
        return nullptr;
    }
    return result;
}

std::shared_ptr<VariableDecl> Parser::variable_decl() {
    std::cout << "not implemented: " << __func__ << std::endl;
    return nullptr;
}

std::shared_ptr<VariableDeclList> Parser::variable_decl_list() {
    std::cout << "not implemented: " << __func__ << std::endl;
    return nullptr;
}

std::shared_ptr<Body> Parser::body() {
    std::cout << "not implemented: " << __func__ << std::endl;
    return nullptr;
}

std::shared_ptr<Statement> Parser::statement() {
    std::cout << "not implemented: " << __func__ << std::endl;
    return nullptr;
}

std::shared_ptr<Statements> Parser::statements() {
    std::cout << "not implemented: " << __func__ << std::endl;
    return nullptr;
}

bool Parser::match(std::vector<Token::Type> types) {
    bool result = true;
    for (auto type : types) {
        if (check(type)) {
            advance();
        } else {
            result = false;
            std::cout << "error: line " << current().line << ": expected " << type << " (between " << previous().type << " and " << peek().type << "), instead got " << current().type << "\n";
            break;
        }
    }
    return result;
}

bool Parser::check(Token::Type type) {
    return current().type == type;
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

}

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
        } else if (*iter == '-') {
            if ((iter + 1) < source.end() && *(iter + 1) == '>') {
                tok.type = Token::Type::ArrowOperator;
                ++iter;
            } else {
                tok.type = Token::Type::MinusOperator;
            }
        } else if (std::isalpha(*iter)) { // identifer
            auto end = std::find_if_not(iter, source.end(), [](char c) { return std::isalnum(c); });
            auto str = std::string(iter, end);
            if (str == "fn") {
                tok.type = Token::Type::FnKeyword;
            } else if (std::find(typenames.begin(), typenames.end(), str) != typenames.end()) {
                tok.type = Token::Type::Typename;
            } else {
                tok.type = Token::Type::Identifier;
                tok.value = str;
            }
            iter = end - 1;
        } else if (*iter == '(') {
            tok.type = Token::Type::OpeningParentheses;
        } else if (*iter == ')') {
            tok.type = Token::Type::ClosingParentheses;
        } else if (*iter == '{') {
            tok.type = Token::Type::OpeningBrace;
        } else if (*iter == '}') {
            tok.type = Token::Type::ClosingBrace;
        } else if (*iter == '=') {
            tok.type = Token::Type::Equals;
        } else if (*iter == '+') {
            tok.type = Token::Type::PlusOperator;
        } else if (*iter == '*') {
            tok.type = Token::Type::MultiplyOperator;
        } else if (*iter == '/') {
            tok.type = Token::Type::DivideOperator;
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
            last_line = tok.line;
        }
        switch (tok.type) {
        case Token::Type::EndOfUnit:
            assert(!"not reachable");
            break;
        case Token::Type::StartOfUnit:
            assert(!"not reachable");
            break;
        case Token::Type::FnKeyword:
            std::cout << "fn ";
            break;
        case Token::Type::ArrowOperator:
            std::cout << "->";
            break;
        case Token::Type::Typename:
            std::cout << "type ";
            break;
        case Token::Type::Equals:
            std::cout << "=";
            break;
        case Token::Type::OpeningBrace:
            std::cout << "{";
            break;
        case Token::Type::ClosingBrace:
            std::cout << "}";
            break;
        case Token::Type::PlusOperator:
            std::cout << "+";
            break;
        case Token::Type::MinusOperator:
            std::cout << "-";
            break;
        case Token::Type::MultiplyOperator:
            std::cout << "*";
            break;
        case Token::Type::DivideOperator:
            std::cout << "/";
            break;
        case Token::Type::Comma:
            std::cout << ",";
            break;
        case Token::Type::Identifier:
            std::cout << "id";
            break;
        case Token::Type::OpeningParentheses:
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

    // syntax check
    AST::Parser parser(tokens);
    auto tree = parser.unit();
    std::cout << "done!\n";
}
