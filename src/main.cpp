#include "ASTParser.h"
#include "Common.h"

#include <cassert>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>

#include <sys/wait.h>
#include <unistd.h>

class Compiler {
public:
    Compiler(std::shared_ptr<AST::Unit> root);
    bool compile(const std::string& original_filename);
    std::string finalize() const { return m_asm.str(); }

private:
    bool is_identifier_known(const std::string& id);
    const std::string& get_register_for_identifier(const std::string& id);
    std::string generate_signature(const std::shared_ptr<AST::FunctionDecl>& func);
    std::string next_register();
    void register_identifier(const std::string& id);

    bool compile_unit(const std::shared_ptr<AST::Unit>&);
    bool compile_function_decl(const std::shared_ptr<AST::FunctionDecl>&);
    bool compile_statement(const std::shared_ptr<AST::Statement>&);
    bool compile_assignment(AST::Assignment*);
    bool compile_expression(const std::shared_ptr<AST::Expression>&, std::string& out_result_reg);
    bool compile_term(const std::shared_ptr<AST::Term>&, std::string& out_result_reg);
    bool compile_operation(const std::string& op, const std::string& left, const std::string& right, std::string& out_reg);
    bool compile_function_call(AST::FunctionCall*, std::string& out);
    bool compile_factor(const std::shared_ptr<AST::Factor>&, std::string& out_reg);
    bool compile_unary(const std::shared_ptr<AST::Unary>&, std::string& out);

    void add_comment(const std::string& comment, bool do_indent = true);
    void add_newline();
    void add_label(const std::string& label);
    void add_instr_ret(const std::string& from);
    void add_instr_mov(const std::string& to, const std::string& from);
    void add_instr_lea(const std::string& to, const std::string& operation);
    void add_push_callee_saved_registers();
    void add_pop_callee_saved_registers();

    std::string tab() const { return "\t"; }
    std::string newline() const { return "\n"; }

    void error(const std::string& what);

    std::shared_ptr<AST::Unit> m_root { nullptr };
    size_t m_current_reg { 0 };
    std::unordered_map<std::string, std::string> m_identifier_register_map;
    static inline const std::vector<std::string> m_registers = {
        "rdi", "rsi", "rdx", "rcx", "r8", "r9",
        "r10", "r11", "r12", "r13", "r14", "r15"
    };
    std::stringstream m_asm;
};

template<typename Base, typename T>
inline bool is_instance_of(const std::shared_ptr<T>&) {
    return std::is_base_of<Base, T>::value;
}

int main(int argc, char** argv) {
    if (argc < 2) {
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

    std::cout << "info: loaded source of size " << source.size() << " bytes.\n";
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
        } else if (std::isalpha(*iter) || *iter == '_') { // identifer
            auto end = std::find_if_not(iter, source.end(), [](char c) { return std::isalnum(c) || c == '_'; });
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
    /* size_t last_line = 1;
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
    std::cout << std::endl;*/

    std::cout << "info: counted " << line - 1 << " lines.\n";
    std::cout << "info: parsed " << tokens.size() << " tokens.\n";

    // syntax check
    AST::Parser parser(tokens);
    auto tree = parser.unit();
    if (argc > 2 && std::string(argv[2]) == "-d") {
        //std::cout << tree->to_string(1) << std::endl;
    }
    std::cout << "info: syntax parser had " << parser.error_count() << " errors." << std::endl;
    if (parser.error_count() > 0) {
        return parser.error_count();
    }

    Compiler compiler(tree);
    compiler.compile(argv[1]);
}

static bool fork_exec(const std::string& command) {
    std::cout << "info: executing: `" << command << "`\n";
    return system(command.c_str()) == 0;
}

Compiler::Compiler(std::shared_ptr<AST::Unit> root)
    : m_root(root) {
}

constexpr const char* custom_start = R"(
; core language _start
_start:
	call main
	mov rdi, rax
	mov rax, 60
	syscall
)";

bool Compiler::compile(const std::string& original_filename) {
    assert(m_root);

    bool ok = compile_unit(m_root);
    if (!ok) {
        std::cout << "error: compilation failed.\n";
        return false;
    }

    auto outfile_name = std::filesystem::path(original_filename).stem().string() + ".asm";
    std::ofstream outfile(outfile_name);
    outfile << "global _start\n"
            << "section .text\n";
    outfile << finalize();
    outfile << custom_start << "\n";
    std::cout << "info: written program to \"" << outfile_name << "\".\n";

    /*
    auto stem = std::filesystem::path(original_filename).stem().string();
    if (!fork_exec("/usr/bin/nasm -s -Wall -felf64 " + stem + ".asm")) {
        std::cout << "nasm failed\n";
        return false;
    }
    if (!fork_exec("/usr/bin/ld -o " + stem + " " + stem + ".o")) {
        std::cout << "ld failed\n";
        return false;
    }*/
    return true;
}

bool Compiler::is_identifier_known(const std::string& id) {
    return std::find_if(m_identifier_register_map.begin(), m_identifier_register_map.end(),
               [&](const auto& pair) -> bool {
                   return pair.first == id;
               })
        != m_identifier_register_map.end();
}

const std::string& Compiler::get_register_for_identifier(const std::string& id) {
    return m_identifier_register_map.at(id);
}

std::string Compiler::generate_signature(const std::shared_ptr<AST::FunctionDecl>& func) {
    std::string res = "fn " + func->name->name;
    res += "(";
    if (func->arguments) {
        for (const auto& arg : func->arguments->variables) {
            res += arg->type_name->name + " " + m_identifier_register_map.at(arg->identifier->name);
            if (arg != func->arguments->variables.back()) {
                res += ",";
            }
        }
    }
    res += ")";
    if (func->result) {
        res += "->" + func->result->type_name->name + " " + m_identifier_register_map.at(func->result->identifier->name);
    }
    return res;
}

std::string Compiler::next_register() {
    assert(m_current_reg < m_registers.size());
    return m_registers[m_current_reg++];
}

void Compiler::register_identifier(const std::string& id) {
    m_identifier_register_map[id] = next_register();
}

bool Compiler::compile_unit(const std::shared_ptr<AST::Unit>& unit) {
    for (const auto& function_decl : unit->decls) {
        bool ok = compile_function_decl(function_decl);
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool Compiler::compile_function_decl(const std::shared_ptr<AST::FunctionDecl>& decl) {
    m_current_reg = 0;
    if (decl->result) {
        m_identifier_register_map[decl->result->identifier->name] = "rax";
    }
    if (decl->arguments) {
        for (const auto& arg : decl->arguments->variables) {
            register_identifier(arg->identifier->name);
        }
    }
    add_newline();
    add_comment(generate_signature(decl), false);
    add_label(decl->name->name);
    add_instr_mov("rax", "0xdeadbeef");
    add_push_callee_saved_registers();
    for (const auto& statement : decl->body->statements->statements) {
        bool ok = compile_statement(statement);
        if (!ok) {
            return false;
        }
    }
    add_pop_callee_saved_registers();
    add_instr_ret(decl->name->name);
    return true;
}

bool Compiler::compile_statement(const std::shared_ptr<AST::Statement>& stmt) {
    if (auto assignment = dynamic_cast<AST::Assignment*>(stmt->statement.get())) {
        bool ok = compile_assignment(assignment);
        if (!ok) {
            return false;
        }
    } else if (auto fncall = dynamic_cast<AST::FunctionCall*>(stmt->statement.get())) {
        std::string ignored_result;
        // TODO: warn ^
        bool ok = compile_function_call(fncall, ignored_result);
        if (!ok) {
            return false;
        }
    } else {
        error("statement is neither assignment nor function call.");
        return false;
    }
    return true;
}

bool Compiler::compile_assignment(AST::Assignment* assignment) {
    std::string expr_result;
    bool ok = compile_expression(assignment->expression, expr_result);
    if (!ok) {
        return false;
    }
    assert(!expr_result.empty());
    add_instr_mov(get_register_for_identifier(assignment->identifier->name), expr_result);
    return true;
}

bool Compiler::compile_expression(const std::shared_ptr<AST::Expression>& expr, std::string& out_result_reg) {
    return compile_term(expr->term, out_result_reg);
}

bool Compiler::compile_term(const std::shared_ptr<AST::Term>& term, std::string& out_result_reg) {
    out_result_reg = next_register();
    std::string next_res;
    bool ok = compile_factor(term->factors.at(0), next_res);
    if (!ok) {
        return false;
    }
    for (size_t i = 1; i < term->factors.size(); ++i) {
        std::string right;
        ok = compile_factor(term->factors.at(i), right);
        if (!ok) {
            return false;
        }
        compile_operation(term->operators.at(i - 1), next_res, right, next_res);
    }
    // add_instr_mov(out_result_reg, next_res);
    out_result_reg = next_res;
    return true;
}

bool Compiler::compile_operation(const std::string& op, const std::string& left, const std::string& right, std::string& out_reg) {
    if (op == "/") {
        assert(!"not implemented: operator '/'");
    }
    auto new_out = next_register();
    add_instr_lea(new_out, "[" + left + std::string(op) + right + "]");
    out_reg = new_out;
    return true;
}

bool Compiler::compile_function_call(AST::FunctionCall* fncall, std::string& out) {
    // TODO: look up & verify signature here
}

bool Compiler::compile_factor(const std::shared_ptr<AST::Factor>& factor, std::string& out_reg) {
    bool ok = compile_unary(factor->unaries.at(0), out_reg);
    if (!ok) {
        return false;
    }
    for (size_t i = 1; i < factor->unaries.size(); ++i) {
        std::string right;
        ok = compile_unary(factor->unaries.at(i), right);
        if (!ok) {
            return false;
        }
        compile_operation(factor->operators.at(i - 1), out_reg, right, out_reg);
    }
    //add_instr_mov(out_reg, next_res);
    return true;
}

bool Compiler::compile_unary(const std::shared_ptr<AST::Unary>& unary, std::string& out) {
    if (!unary->op.empty()) {
        assert(unary->op == "-");
        assert(!"not implemented");
    }
    // we know its a primary since it's only a unary if there was a '-', which is not implemented
    if (auto primary = dynamic_cast<AST::Primary*>(unary->unary_or_primary.get())) {
        if (auto numeric_literal = dynamic_cast<AST::NumericLiteral*>(primary->value.get())) {
            out = std::to_string(numeric_literal->value);
        } else if (auto numeric_literal = dynamic_cast<AST::StringLiteral*>(primary->value.get())) {
            assert(!"not implemented");
        } else if (auto identifier = dynamic_cast<AST::Identifier*>(primary->value.get())) {
            out = get_register_for_identifier(identifier->name);
        } else if (auto grouped_expression = dynamic_cast<AST::GroupedExpression*>(primary->value.get())) {
            return compile_expression(grouped_expression->expression, out);
        } else if (auto fncall = dynamic_cast<AST::FunctionCall*>(primary->value.get())) {
            return compile_function_call(fncall, out);
        } else {
            assert(!"unreachable code reached");
        }
    } else {
        assert(!"unreachable code reached");
    }
    return true;
}

void Compiler::add_comment(const std::string& comment, bool do_indent) {
    if (do_indent) {
        m_asm << tab();
    }
    m_asm << "; " << comment << newline();
}

void Compiler::add_newline() {
    m_asm << newline();
}

void Compiler::add_label(const std::string& label) {
    m_asm << label << ":" << newline();
}

void Compiler::add_instr_ret(const std::string& from) {
    add_comment("return from " + from);
    m_asm << tab() << "ret" << newline();
}

void Compiler::add_instr_mov(const std::string& to, const std::string& from) {
    m_asm << tab() << "mov " << to << ", " << from << newline();
}

void Compiler::add_instr_lea(const std::string& to, const std::string& operation) {
    m_asm << tab() << "lea " << to << ", " << operation << newline();
}

void Compiler::add_push_callee_saved_registers() {
    m_asm << tab() << "push rbx" << newline()
          << tab() << "push rsp" << newline()
          << tab() << "push rbp" << newline()
          << tab() << "push r12" << newline()
          << tab() << "push r13" << newline()
          << tab() << "push r14" << newline()
          << tab() << "push r15" << newline();
}

void Compiler::add_pop_callee_saved_registers() {
    m_asm << tab() << "pop r15" << newline()
          << tab() << "pop r14" << newline()
          << tab() << "pop r13" << newline()
          << tab() << "pop r12" << newline()
          << tab() << "pop rbp" << newline()
          << tab() << "pop rsp" << newline()
          << tab() << "pop rbx" << newline();
}

void Compiler::error(const std::string& what) {
    std::cout << "error: compiler: " << what << "\n";
}
