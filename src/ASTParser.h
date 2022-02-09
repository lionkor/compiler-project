#pragma once

#include "Common.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

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
struct FunctionCall;
struct UseDecl;
struct IfStatement;
struct ElseStatement;

struct Node {
    virtual ~Node() { }
    virtual std::string to_string(size_t) { return "Node{}"; }
};

struct FunctionCall : public Node {
    std::shared_ptr<Identifier> name;
    std::vector<std::shared_ptr<Expression>> arguments;
    virtual std::string to_string(size_t level);
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
    std::vector<std::shared_ptr<UseDecl>> use_decls;
    virtual std::string to_string(size_t level);
};

struct UseDecl : public Node {
    std::string path;
    virtual std::string to_string(size_t level);
};

struct IfStatement : public Node {
    std::shared_ptr<Expression> condition;
    std::shared_ptr<Body> body;
    std::shared_ptr<ElseStatement> else_statement;
    virtual std::string to_string(size_t level);
};

struct ElseStatement : public Node {
    std::shared_ptr<Body> body;
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
    std::shared_ptr<IfStatement> if_statement();
    std::shared_ptr<ElseStatement> else_statement();
    std::shared_ptr<Statements> statements();
    std::shared_ptr<Assignment> assignment();
    std::shared_ptr<Identifier> identifier();
    std::shared_ptr<Expression> expression();
    std::shared_ptr<FunctionCall> function_call();
    std::shared_ptr<Term> term();
    std::shared_ptr<Factor> factor();
    std::shared_ptr<Unary> unary();
    std::shared_ptr<Primary> primary();
    std::shared_ptr<GroupedExpression> grouped_expression();
    std::shared_ptr<Node> literal();
    std::shared_ptr<NumericLiteral> numeric_literal();
    std::shared_ptr<StringLiteral> string_literal();
    std::shared_ptr<Typename> type_name();
    std::shared_ptr<UseDecl> use_decl();

    size_t error_count() const { return m_error_count; }
    void errors_off() { m_errors_enabled = false; }
    void errors_on() { m_errors_enabled = true; }

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
    bool m_errors_enabled { true };
    size_t m_error_count { 0 };
};

}
