#include <String.h>
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <variant>

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

struct Statement;
struct Expression;
struct Assignment;
struct Statements;
struct Term;
struct Body;
struct Identifier;
struct Typename;
struct VariableDecl;
struct VariableDeclList;
struct FunctionDecl;
struct Factor;
struct Unit;
struct Unary;
struct Primary;
struct GroupedExpression;

struct Node {
    virtual ~Node() { }
    virtual std::string to_string(size_t) { return "Node{}"; }
};

struct Statement : public Node {
    std::shared_ptr<Node> statement;
    virtual std::string to_string(size_t level);
};

struct Expression : public Node {
    std::shared_ptr<Term> term;
    virtual std::string to_string(size_t level);
};

struct Term : public Node {
    std::vector<std::shared_ptr<Factor>> factors;
    std::vector<std::string> operators; // one less than factors
    virtual std::string to_string(size_t level);
};

struct Factor : public Node {
    std::vector<std::shared_ptr<Unary>> unaries;
    std::vector<std::string> operators; // one less than unaries
    virtual std::string to_string(size_t level);
};

struct Unary : public Node {
    std::string op;
    std::shared_ptr<Node> unary_or_primary;
    virtual std::string to_string(size_t level);
};

struct Primary : public Node {
    std::shared_ptr<Node> value; // literal, identifier or grouped expression
    virtual std::string to_string(size_t level);
};

struct GroupedExpression : public Node {
    std::shared_ptr<Expression> expression;
    virtual std::string to_string(size_t level);
};

struct Assignment : public Node {
    std::shared_ptr<Identifier> identifier;
    std::shared_ptr<Expression> expression;
    virtual std::string to_string(size_t level);
};

struct Statements : public Node {
    std::vector<std::shared_ptr<Statement>> statements;
    virtual std::string to_string(size_t level);
};

struct Body : public Node {
    std::shared_ptr<Statements> statements;
    virtual std::string to_string(size_t level);
};

struct Identifier : public Node {
    std::string name;
    virtual std::string to_string(size_t level);
};

struct Typename : public Node {
    std::string name;
    virtual std::string to_string(size_t level);
};

struct NumericLiteral : public Node {
    size_t value;
    virtual std::string to_string(size_t level);
};

struct StringLiteral : public Node {
    std::string value;
    virtual std::string to_string(size_t level);
};

struct VariableDecl : public Node {
    std::shared_ptr<Identifier> identifier;
    std::shared_ptr<Typename> type_name;
    virtual std::string to_string(size_t level);
};

struct VariableDeclList : public Node {
    std::vector<std::shared_ptr<VariableDecl>> variables;
    virtual std::string to_string(size_t level);
};

struct FunctionDecl : public Node {
    std::shared_ptr<Identifier> name;
    std::shared_ptr<VariableDeclList> arguments;
    std::shared_ptr<VariableDecl> result;
    std::shared_ptr<Body> body;
    virtual std::string to_string(size_t level);
};

struct Unit : public Node {
    std::vector<std::shared_ptr<FunctionDecl>> decls;
    virtual std::string to_string(size_t level);
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
    std::shared_ptr<Assignment> assignment();
    std::shared_ptr<Identifier> identifier();
    std::shared_ptr<Expression> expression();
    std::shared_ptr<Term> term();
    std::shared_ptr<Factor> factor();
    std::shared_ptr<Unary> unary();
    std::shared_ptr<Primary> primary();
    std::shared_ptr<GroupedExpression> grouped_expression();
    std::shared_ptr<Node> literal();
    std::shared_ptr<NumericLiteral> numeric_literal();
    std::shared_ptr<StringLiteral> string_literal();
    std::shared_ptr<Typename> type_name();

    void errors_off() { bool m_errors = false; }
    void errors_on() { bool m_errors = true; }

private:
    bool match(std::vector<Token::Type>);
    bool check(Token::Type);
    bool check_any_of(std::vector<Token::Type>);
    void advance() { ++m_i; }
    Token previous();
    Token peek();
    const Token& current() { return m_tokens[m_i]; }
    void error(const std::string& what);
    void error_expected(Token::Type expected);
    size_t m_i { 0 };
    std::vector<Token> m_tokens;
    bool m_errors { true };
};

std::shared_ptr<Unit> Parser::unit() {
    auto result = std::make_shared<Unit>();
    std::shared_ptr<FunctionDecl> fn;
    for (;;) {
        if (peek().type == Token::Type::EndOfUnit) {
            break;
        }
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
    result->statement = assignment();
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
        result->value = identifier();
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
    if (m_errors) {
        std::cout << "error: line " << current().line << ": " << what << std::endl;
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
            result += "|";
        }
        result += " ";
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
    for (size_t i = 0; i < factors.size(); ++i) {
        if ((i + 1) % 2 == 0) {
            res += indent(level) + "operator " + operators.at(i - 1) + "\n";
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
    for (auto& fn : decls) {
        res += indent(level) + fn->to_string(level + 1);
    }
    return res;
}

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << argv[0] << ": missing argument" << std::endl;
        return 1;
    }
    FILE* file = std::fopen(argv[1], "r");
    if (!file) {
        std::cout << argv[0] << ": failed to open \"" << argv[1] << "\": " << std::strerror(errno) << "\n";
        return 1;
    }
    std::string source;
    source.resize(std::filesystem::file_size(argv[1]));
    std::fread(source.data(), 1, source.size(), file);
    std::fclose(file);
    
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
                tok.value = *iter;
            }
        } else if (std::isalpha(*iter)) { // identifer
            auto end = std::find_if_not(iter, source.end(), [](char c) { return std::isalnum(c); });
            auto str = std::string(iter, end);
            if (str == "fn") {
                tok.type = Token::Type::FnKeyword;
            } else if (std::find(typenames.begin(), typenames.end(), str) != typenames.end()) {
                tok.type = Token::Type::Typename;
                tok.value = str;
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
            tok.value = *iter;
        } else if (*iter == '*') {
            tok.type = Token::Type::MultiplyOperator;
            tok.value = *iter;
        } else if (*iter == '/') {
            tok.type = Token::Type::DivideOperator;
            tok.value = *iter;
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
    std::cout << tree->to_string(1) << std::endl;
}
