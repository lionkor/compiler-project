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

private:
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
    void add_instr(const std::string& instr);
    void add_instr_ret(const std::string& from);
    void add_instr_mov(const std::string& to, const std::string& from);
    void add_instr_add(const std::string& to, const std::string& from);
    void add_instr_sub(const std::string& a, const std::string& b);
    void add_instr_mul(const std::string& a, const std::string& b);
    void add_instr_lea(const std::string& to, const std::string& operation);
    void add_instr_call(const std::string& label);
    void add_push_callee_saved_registers();
    void add_pop_callee_saved_registers();

    std::string tab() const { return "\t"; }

    void error(const std::string& what);

    bool is_identifier_known(const std::string& id);
    size_t get_address_for_identifier(const std::string& id);
    std::string generate_signature(const std::shared_ptr<AST::FunctionDecl>& func);
    size_t register_identifier(const std::string& id, size_t size);
    size_t make_stack_ptr_for_size(size_t size);

    std::shared_ptr<AST::Unit> m_root { nullptr };
    size_t m_current_reg { 0 };
    std::vector<std::string> m_asm;
    size_t m_current_stack_ptr { 0 };
    std::unordered_map<std::string, size_t> m_identifier_stack_addr_map;

    static inline const std::string m_arg_registers[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
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
    outfile << R"(
syscall:
	syscall
)";
    for (const auto& line : m_asm) {
        outfile << line << "\n";
    }
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
    return std::find_if(m_identifier_stack_addr_map.begin(), m_identifier_stack_addr_map.end(),
               [&](const auto& pair) -> bool {
                   return pair.first == id;
               })
        != m_identifier_stack_addr_map.end();
}

size_t Compiler::get_address_for_identifier(const std::string& id) {
    return m_identifier_stack_addr_map.at(id);
}

std::string Compiler::generate_signature(const std::shared_ptr<AST::FunctionDecl>& func) {
    std::string res = "fn " + func->name->name;
    res += "(";
    if (func->arguments) {
        for (const auto& arg : func->arguments->variables) {
            res += arg->type_name->name + " " + arg->identifier->name;
            if (arg != func->arguments->variables.back()) {
                res += ",";
            }
        }
    }
    res += ")";
    if (func->result) {
        res += "->" + func->result->type_name->name + " " + func->result->identifier->name;
    }
    return res;
}

size_t Compiler::register_identifier(const std::string& id, size_t size) {
    auto addr = make_stack_ptr_for_size(size);
    m_identifier_stack_addr_map[id] = addr;
    return addr;
}

size_t Compiler::make_stack_ptr_for_size(size_t size) {
    return m_current_stack_ptr += size;
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
    m_current_stack_ptr = 0;
    add_newline();
    add_comment(generate_signature(decl), false);
    add_label(decl->name->name);
    add_push_callee_saved_registers();
    add_instr("push rbp");
    add_instr("mov rbp, rsp");
    size_t fn_start_index = m_asm.size();
    std::string return_value_storage = "0";
    if (decl->result) {
        auto offset = register_identifier(decl->result->identifier->name, 8);
        return_value_storage = "rbp-" + std::to_string(offset);
        add_comment(return_value_storage + " = " + decl->result->identifier->name);
        add_comment("setting " + return_value_storage + " to debug value");
        add_instr_mov("rax", "0xdeadbeef");
        add_instr_mov(return_value_storage, "rax");
    }
    if (decl->arguments) {
        size_t i = 0;
        for (const auto& arg : decl->arguments->variables) {
            auto offset = register_identifier(arg->identifier->name, 8);
            auto reg = "rbp-" + std::to_string(offset);
            add_comment(reg + " = " + arg->identifier->name);
            add_instr_mov(reg, m_arg_registers[i]);
            ++i;
        }
    }
    for (const auto& statement : decl->body->statements->statements) {
        bool ok = compile_statement(statement);
        if (!ok) {
            return false;
        }
    }
    add_pop_callee_saved_registers();
    add_instr_mov("rax", return_value_storage);
    add_instr("leave");
    add_instr_ret(decl->name->name);
    m_asm.insert(m_asm.begin() + fn_start_index, tab() + "sub rsp, " + std::to_string(m_current_stack_ptr));
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
    add_comment(assignment->identifier->name + " = " + expr_result);
    if (!ok) {
        return false;
    }
    assert(!expr_result.empty());
    add_instr_mov("rbp-" + std::to_string(get_address_for_identifier(assignment->identifier->name)), expr_result);
    return true;
}

bool Compiler::compile_expression(const std::shared_ptr<AST::Expression>& expr, std::string& out_result_reg) {
    return compile_term(expr->term, out_result_reg);
}

bool Compiler::compile_term(const std::shared_ptr<AST::Term>& term, std::string& out_result_reg) {
    out_result_reg = "rbp-" + std::to_string(make_stack_ptr_for_size(8));
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
    std::string left_copy = left;
    std::string right_copy = right;
    out_reg = "rbx";
    add_comment(out_reg + " = " + left_copy + " " + op + " " + right_copy);
    add_instr_mov("rax", left_copy);
    if (op == "+") {
        add_instr_add("rax", right_copy);
    } else if (op == "-") {
        add_instr_sub("rax", right_copy);
    } else if (op == "*") {
        add_instr_mul("rax", right_copy);
    } else {
        assert(!"not implemented");
    }
    add_instr_mov(out_reg, "rax");
    return true;
}

bool Compiler::compile_function_call(AST::FunctionCall* fncall, std::string& out) {
    // TODO: look up & verify signature here
    size_t i = 0;
    for (const auto& arg : fncall->arguments) {
        std::string expr_out;
        compile_expression(arg, expr_out);
        add_instr_mov(m_arg_registers[i], expr_out);
        ++i;
    }
    add_instr_call(fncall->name->name);
    out = "rax";
    return true;
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
            out = "rbp-" + std::to_string(get_address_for_identifier(identifier->name));
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
    std::string line;
    if (do_indent) {
        line += tab();
    }
    line += "; " + comment;
    m_asm.push_back(line);
}

void Compiler::add_newline() {
    m_asm.push_back("");
}

void Compiler::add_label(const std::string& label) {
    m_asm.push_back(label + ":");
}

void Compiler::add_instr(const std::string& instr) {
    m_asm.push_back(tab() + instr);
}

void Compiler::add_instr_ret(const std::string& from) {
    add_comment("return from " + from);
    m_asm.push_back(tab() + "ret");
}

void Compiler::add_instr_mov(const std::string& to, const std::string& from) {
    std::string real_to = to;
    std::string real_from = from;
    if (to.substr(0, 3) == "rbp") {
        real_to = "qword [" + real_to + "]";
    }
    if (from.substr(0, 3) == "rbp") {
        real_from = "qword [" + real_from + "]";
    }
    m_asm.push_back(tab() + "mov " + real_to + ", " + real_from);
}

// TODO: this needs to be a function that gets called by add and mov, since they're the same.
void Compiler::add_instr_add(const std::string& to, const std::string& from) {
    std::string real_to = to;
    std::string real_from = from;
    if (to.substr(0, 3) == "rbp") {
        real_to = "qword [" + real_to + "]";
    }
    if (from.substr(0, 3) == "rbp") {
        real_from = "qword [" + real_from + "]";
    }
    m_asm.push_back(tab() + "add " + real_to + ", " + real_from);
}

void Compiler::add_instr_sub(const std::string& a, const std::string& b) {
    std::string real_a = a;
    std::string real_b = b;
    if (a.substr(0, 3) == "rbp") {
        real_a = "qword [" + real_a + "]";
    }
    if (b.substr(0, 3) == "rbp") {
        real_b = "qword [" + real_b + "]";
    }
    m_asm.push_back(tab() + "sub " + real_a + ", " + real_b);
}

void Compiler::add_instr_mul(const std::string& a, const std::string& b) {
    std::string real_a = a;
    std::string real_b = b;
    if (a.substr(0, 3) == "rbp") {
        real_a = "qword [" + real_a + "]";
    }
    if (b.substr(0, 3) == "rbp") {
        real_b = "qword [" + real_b + "]";
    }
    m_asm.push_back(tab() + "imul " + real_a + ", " + real_b);
}

void Compiler::add_instr_lea(const std::string& to, const std::string& operation) {
    m_asm.push_back(tab() + "lea " + to + ", " + operation);
}

void Compiler::add_instr_call(const std::string& label) {
    m_asm.push_back(tab() + "call " + label);
}

void Compiler::add_push_callee_saved_registers() {
}

void Compiler::add_pop_callee_saved_registers() {
}

void Compiler::error(const std::string& what) {
    std::cout << "error: compiler: " << what << "\n";
}
