#include "ASTParser.h"
#include "Common.h"
#include "Type.h"

#include <lk/Logger.h>

#include <cassert>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include <sys/wait.h>
#include <unistd.h>

class Object {
public:
    Object(const std::shared_ptr<AST::Unit>& root);
    bool compile(const std::string& original_filename, bool standalone);

    const std::vector<std::unique_ptr<Object>>& dependencies() const;
    const std::string& obj_file() const;
    const std::vector<std::string>& globals() const;
    bool get_type_by_name(Type& out_type, const std::string& type_name) const;

private:
    bool compile_unit(const std::shared_ptr<AST::Unit>&);
    bool compile_function_decl(const std::shared_ptr<AST::FunctionDecl>&);
    bool compile_body(const std::shared_ptr<AST::Body>&);
    bool compile_statement(const std::shared_ptr<AST::Statement>&);
    bool compile_if_statement(const AST::IfStatement*);
    bool compile_else_statement(const std::shared_ptr<AST::ElseStatement>& stmt);
    bool compile_variable_decl(const AST::VariableDecl*);
    bool compile_assignment(const AST::Assignment*);
    bool compile_expression(const std::shared_ptr<AST::Expression>&, std::string& out_result_reg);
    bool compile_term(const std::shared_ptr<AST::Term>&, std::string& out_result_reg);
    bool compile_operation(const std::string& op, const std::string& left, const std::string& right, std::string& out_reg);
    bool compile_function_call(AST::FunctionCall*, std::string& out);
    bool compile_factor(const std::shared_ptr<AST::Factor>&, std::string& out_reg);
    bool compile_unary(const std::shared_ptr<AST::Unary>&, std::string& out);
    bool compile_use_decl(const std::shared_ptr<AST::UseDecl>& unit);

    void add_comment(const std::string& comment, bool do_indent = true);
    void add_newline();
    void add_label(const std::string& label);
    void add_instr(const std::string& instr);
    void add_instr_ret(const std::string& from);
    void add_instr_mov(const std::string& to, const std::string& from);
    void add_instr_cmp(const std::string& a, const std::string& b);
    void add_instr_add(const std::string& to, const std::string& from);
    void add_instr_sub(const std::string& a, const std::string& b);
    void add_instr_mul(const std::string& a, const std::string& b);
    void add_instr_lea(const std::string& to, const std::string& operation);
    void add_instr_call(const std::string& label);
    void add_push_callee_saved_registers();
    void add_pop_callee_saved_registers();

    std::string tab() const { return "    "; }

    void error(const std::string& what);

    bool is_identifier_known(const std::string& id);
    size_t get_address_for_identifier(const std::string& id);
    Type get_type_for_identifier(const std::string& id);
    std::string generate_signature(const std::shared_ptr<AST::FunctionDecl>& func);
    size_t register_identifier(const std::string& id, Type type);
    size_t make_stack_ptr_for_size(size_t size);
    std::string generate_unique_label();

    std::shared_ptr<AST::Unit> m_root { nullptr };
    size_t m_current_reg { 0 };
    std::vector<std::string> m_asm_text;
    std::vector<std::string> m_asm_data;
    size_t m_current_stack_ptr { 0 };
    std::unordered_map<std::string, size_t> m_identifier_stack_addr_map;

    std::vector<std::string> m_globals;
    size_t m_unique_label_i { 0 };
    std::vector<std::unique_ptr<Object>> m_dependencies {};
    std::string m_obj_file;

    std::unordered_set<Type> m_types {};
    std::unordered_map<std::string, Type> m_identifiers {};

    static inline const std::string m_arg_registers[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
};

template<typename Base, typename T>
inline bool is_instance_of(const std::shared_ptr<T>&) {
    return std::is_base_of<Base, T>::value;
}

static std::vector<Token> tokenize(const std::string& source);

static std::unique_ptr<Object> compile_source_to_obj(const std::string filename, bool standalone, bool debug = true);

static void add_objs_from_obj(const Object& obj, std::unordered_set<std::string>& objs) {
    objs.insert(obj.obj_file());
    for (const auto& dependency : obj.dependencies()) {
        add_objs_from_obj(*dependency, objs);
    }
}

int main(int argc, char** argv) {
    lk::Logger::the().add_stream(std::cout);
    lk::Logger::the().add_file_stream("compiler.log");

    if (argc < 2) {
        lk::log::error() << argv[0] << ": missing argument" << std::endl;
        return 1;
    }

    auto obj = compile_source_to_obj(argv[1], true);

    lk::log::info() << "linking " << obj->obj_file() << " with " << obj->dependencies().size() << " dependencies..." << std::endl;

    std::string src = argv[1];
    std::string final = (std::filesystem::path(src).parent_path() / std::filesystem::path(src).stem()).string();

    std::unordered_set<std::string> objs;
    add_objs_from_obj(*obj, objs);

    std::string link_command = "ld -o " + final;
    for (const auto& name : objs) {
        link_command += " " + name;
    }

    lk::log::info() << "running: " << link_command << std::endl;
    if (WEXITSTATUS(std::system(link_command.c_str())) != 0) {
        lk::log::error() << "ld failed\n";
        return -1;
    }
}

static std::unique_ptr<Object> compile_source_to_obj(const std::string path, bool standalone, bool debug) {
    FILE* file = std::fopen(path.data(), "r");
    if (!file) {
        lk::log::error() << "failed to open \"" << path << "\": " << std::strerror(errno) << "\n";
        return nullptr;
    }
    std::string source;
    source.resize(std::filesystem::file_size(path));
    std::fread(source.data(), 1, source.size(), file);
    std::fclose(file);

    lk::log::info() << "loaded source of size " << source.size() << " bytes.\n";

    auto tokens = tokenize(source);
    // syntax check
    AST::Parser parser(tokens);
    auto tree = parser.unit();
    if (debug) {
        lk::log::debug() << "\n"
                         << tree->to_string(1) << std::endl;
    }
    lk::log::info() << "syntax parser had " << parser.error_count() << " errors." << std::endl;
    if (parser.error_count() > 0) {
        return nullptr;
    }

    auto object = std::make_unique<Object>(tree);
    if (!object->compile(path, standalone)) {
        lk::log::error() << "failed to compile \"" << path << "\"" << std::endl;
        return nullptr;
    }
    return object;
}

static std::vector<Token> tokenize(const std::string& source) {
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
            } else if (str == "use") {
                tok.type = Token::Type::UseKeyword;
            } else if (str == "if") {
                tok.type = Token::Type::IfKeyword;
            } else if (str == "else") {
                tok.type = Token::Type::ElseKeyword;
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
            size_t value {};
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
                lk::log::warning() << line << ": end of file before end of string literal!\n";
                continue;
            }
            tok.type = Token::Type::StringLiteral;
            tok.value = std::string(iter + 1, end);
            iter = end;
        } else {
            lk::log::error() << line << ": couldn't parse: " << std::string(&*iter) << "\n";
            continue;
        }
        tokens.push_back(std::move(tok));
    }

    lk::log::info() << "counted " << line - 1 << " lines.\n";
    lk::log::info() << "parsed " << tokens.size() << " tokens.\n";

    return tokens;
}

Object::Object(const std::shared_ptr<AST::Unit>& root)
    : m_root(root) {
    m_types.insert(s_builtin_types.begin(), s_builtin_types.end());
}

constexpr const char* libasm_decl = R"(
; all globals, asm decls
%include "asm/extern.asm"
)";

constexpr const char* libasm = R"(
; libasm
%include "asm/lib.asm"
)";

constexpr const char* custom_start = R"(
; core language _start
%include "asm/_start.asm"
)";

bool Object::compile(const std::string& original_filename, bool standalone) {
    assert(m_root);

    bool ok = compile_unit(m_root);
    if (!ok) {
        lk::log::error() << "compilation failed.\n";
        return false;
    }

    auto stem = std::filesystem::path(original_filename).parent_path() / std::filesystem::path(original_filename).stem();

    auto outfile_name = stem.string() + ".asm";
    { // ofstream scope
        std::ofstream outfile(outfile_name);
        if (standalone) {
            outfile << "global _start\n";
        }
        outfile << "\nsection .data\n";

        // write all known globals of dependencies
        for (const auto& dep : dependencies()) {
            outfile << "\t; externs from dependency \"" << dep->obj_file() << "\"\n";
            for (const auto& global : dep->globals()) {
                outfile << "\textern " << global << "\n";
            }
        }

        // write own globals
        outfile << "\t; own globals\n";
        for (const auto& global : globals()) {
            outfile << "\tglobal " << global << "\n";
        }

        outfile << "\t; own data\n";
        for (const auto& line : m_asm_data) {
            outfile << line << "\n";
        }

        outfile << "\nsection .text\n";
        if (standalone) {
            outfile << libasm;
        } else {
            outfile << libasm_decl;
        }

        // TODO syscall missing one argument
        for (const auto& line : m_asm_text) {
            outfile << line << "\n";
        }
        if (standalone) {
            outfile << custom_start << "\n";
        }
    }

    m_obj_file = stem.string() + ".o";
    std::string compile_cmd = "nasm " + stem.string() + ".asm -o " + m_obj_file + ".asm -Wall -g -felf64 -I. -E";
    lk::log::info() << "running: " << compile_cmd << std::endl;
    if (WEXITSTATUS(std::system(compile_cmd.c_str())) != 0) {
        lk::log::info() << "nasm -E failed\n";
        return false;
    }
    std::string debug_compile_cmd = "nasm " + stem.string() + ".asm -o " + m_obj_file + " -Wall -g -felf64 -I.";
    lk::log::info() << "running: " << debug_compile_cmd << std::endl;
    if (WEXITSTATUS(std::system(debug_compile_cmd.c_str())) != 0) {
        lk::log::info() << "nasm failed\n";
        return false;
    }

    lk::log::info() << "successfully compiled \"" << original_filename << "\" to \"" << m_obj_file << "\"" << std::endl;
    return true;
}

bool Object::is_identifier_known(const std::string& id) {
    return m_identifiers.contains(id) && m_identifier_stack_addr_map.contains(id);
}

size_t Object::get_address_for_identifier(const std::string& id) {
    return m_identifier_stack_addr_map.at(id);
}

Type Object::get_type_for_identifier(const std::string& id) {
    return m_identifiers.at(id);
}

std::string Object::generate_signature(const std::shared_ptr<AST::FunctionDecl>& func) {
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

size_t Object::register_identifier(const std::string& id, Type type) {
    lk::log::debug() << "identifier '" << id << "' is type: " << type << std::endl;
    m_identifiers[id] = type;
    auto addr = make_stack_ptr_for_size(type.size);
    m_identifier_stack_addr_map[id] = addr;
    return addr;
}

size_t Object::make_stack_ptr_for_size(size_t size) {
    return m_current_stack_ptr += size;
}

std::string Object::generate_unique_label() {
    std::string name = m_obj_file;
    for (char& c : name) {
        if (!isalnum(c)) {
            c = '_';
        }
    }
    return "__" + name + "_" + std::to_string(m_unique_label_i++);
}

const std::vector<std::string>& Object::globals() const {
    return m_globals;
}

bool Object::get_type_by_name(Type& out_type, const std::string& type_name) const {
    auto iter = std::find_if(m_types.begin(), m_types.end(), [&type_name](const Type& type) { return type.name == type_name; });
    if (iter != m_types.end()) {
        out_type = *iter;
        return true;
    } else {
        return false;
    }
}

const std::string& Object::obj_file() const {
    return m_obj_file;
}

const std::vector<std::unique_ptr<Object>>& Object::dependencies() const {
    return m_dependencies;
}

bool Object::compile_use_decl(const std::shared_ptr<AST::UseDecl>& unit) {
    auto ptr = compile_source_to_obj(unit->path + ".xc", false);
    if (!ptr) {
        lk::log::error() << "failed to compile dependency \"" << unit->path << "\"" << std::endl;
        return false;
    }
    m_dependencies.emplace_back(std::move(ptr));
    return true;
}

bool Object::compile_unit(const std::shared_ptr<AST::Unit>& unit) {
    for (const auto& use_decl : unit->use_decls) {
        bool ok = compile_use_decl(use_decl);
        if (!ok) {
            return false;
        }
    }
    for (const auto& function_decl : unit->decls) {
        bool ok = compile_function_decl(function_decl);
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool Object::compile_function_decl(const std::shared_ptr<AST::FunctionDecl>& decl) {
    m_current_reg = 0;
    m_current_stack_ptr = 0;
    m_globals.push_back(decl->name->name);
    add_newline();
    add_comment(generate_signature(decl), false);
    add_label(decl->name->name);
    add_push_callee_saved_registers();
    add_instr("push rbp");
    add_instr("mov rbp, rsp");
    size_t fn_start_index = m_asm_text.size();
    std::string return_value_storage = "0";
    if (decl->result) {
        Type result_type;
        if (!get_type_by_name(result_type, decl->result->type_name->name)) {
            lk::log::error() << "'" << decl->result->type_name->name << "' is not a known type" << std::endl;
            return false;
        }
        auto offset = register_identifier(decl->result->identifier->name, result_type);
        return_value_storage = "rbp-" + std::to_string(offset);
        add_comment(return_value_storage + " = " + decl->result->identifier->name);
        add_comment("setting " + return_value_storage + " to debug value");
        add_instr_mov("rax", "0xdeadc0de");
        add_instr_mov(return_value_storage, "rax");
    }
    if (decl->arguments) {
        size_t i = 0;
        for (const auto& arg : decl->arguments->variables) {
            Type var_type;
            if (!get_type_by_name(var_type, arg->type_name->name)) {
                lk::log::error() << "'" << arg->type_name->name << "' is not a known type" << std::endl;
                return false;
            }
            auto offset = register_identifier(arg->identifier->name, var_type);
            auto reg = "rbp-" + std::to_string(offset);
            add_comment(reg + " = " + arg->identifier->name);
            add_instr_mov(reg, m_arg_registers[i]);
            ++i;
        }
    }
    bool ok = compile_body(decl->body);
    if (!ok) {
        return false;
    }
    add_pop_callee_saved_registers();
    add_instr_mov("rax", return_value_storage);
    add_instr("leave");
    add_instr_ret(decl->name->name);
    m_asm_text.insert(m_asm_text.begin() + fn_start_index, tab() + "sub rsp, " + std::to_string(m_current_stack_ptr));
    return true;
}

bool Object::compile_body(const std::shared_ptr<AST::Body>& body) {
    for (const auto& statement : body->statements->statements) {
        bool ok = compile_statement(statement);
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool Object::compile_statement(const std::shared_ptr<AST::Statement>& stmt) {
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
    } else if (auto decl = dynamic_cast<AST::VariableDecl*>(stmt->statement.get())) {
        bool ok = compile_variable_decl(decl);
        if (!ok) {
            return false;
        }
    } else if (auto if_stmt = dynamic_cast<AST::IfStatement*>(stmt->statement.get())) {
        bool ok = compile_if_statement(if_stmt);
        if (!ok) {
            return false;
        }
    } else {
        error("statement is not assignment, function call, or if statement, but should be.");
        return false;
    }
    return true;
}

bool Object::compile_if_statement(const AST::IfStatement* stmt) {
    std::string cond_result;
    add_comment("condition of if-statement");
    bool ok = compile_expression(stmt->condition, cond_result);
    if (!ok) {
        return false;
    }
    std::string else_label = generate_unique_label();
    std::string end_label = generate_unique_label();
    add_instr("push rax");
    add_instr_mov("rax", cond_result);
    add_instr_cmp("rax", "0");
    add_instr("pop rax");
    add_comment("jump to else/end");
    add_instr("je " + else_label);
    add_comment("if body");
    ok = compile_body(stmt->body);
    if (!ok) {
        return false;
    }
    if (stmt->else_statement) {
        add_comment("jump to end, past the else");
        add_instr("jmp " + end_label);
        add_label(else_label);
        ok = compile_else_statement(stmt->else_statement);
        if (!ok) {
            return false;
        }
        add_label(end_label);
    } else {
        add_label(else_label);
    }
    return true;
}

bool Object::compile_else_statement(const std::shared_ptr<AST::ElseStatement>& stmt) {
    add_comment("else body");
    bool ok = compile_body(stmt->body);
    return ok;
}

bool Object::compile_variable_decl(const AST::VariableDecl* decl) {
    Type var_type;
    if (!get_type_by_name(var_type, decl->type_name->name)) {
        lk::log::error() << "type '" << decl->type_name->name << "' for variable '" << decl->identifier->name << "' is not known" << std::endl;
        return false;
    }
    auto addr = register_identifier(decl->identifier->name, var_type);
    add_comment("rbp-" + std::to_string(addr) + " = " + decl->type_name->name + " " + decl->identifier->name);
    return true;
}

bool Object::compile_assignment(const AST::Assignment* assignment) {
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

bool Object::compile_expression(const std::shared_ptr<AST::Expression>& expr, std::string& out_result_reg) {
    return compile_term(expr->term, out_result_reg);
}

bool Object::compile_term(const std::shared_ptr<AST::Term>& term, std::string& out_result_reg) {
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

bool Object::compile_operation(const std::string& op, const std::string& left, const std::string& right, std::string& out_reg) {
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

bool Object::compile_function_call(AST::FunctionCall* fncall, std::string& out) {
    add_comment("setup arguments to " + fncall->name->name + "()");
    std::vector<std::string> arg_stack;
    arg_stack.reserve(fncall->arguments.size());
    size_t i = 0;
    for (const auto& arg : fncall->arguments) {
        std::string arg_stack_element = "rbp-" + std::to_string(make_stack_ptr_for_size(8));
        std::string expr_out;
        compile_expression(arg, expr_out);
        add_comment(fncall->name->name + "() arg " + std::to_string(i) + " is " + arg_stack_element);
        add_instr_mov(arg_stack_element, expr_out);
        arg_stack.push_back(arg_stack_element);
        ++i;
    }
    i = 0;
    for (const auto& arg : arg_stack) {
        add_instr_mov(m_arg_registers[i], arg);
        ++i;
    }
    add_comment("call to " + fncall->name->name + "()");
    add_instr_call(fncall->name->name);
    out = "rax";
    return true;
}

bool Object::compile_factor(const std::shared_ptr<AST::Factor>& factor, std::string& out_reg) {
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
    // add_instr_mov(out_reg, next_res);
    return true;
}

bool Object::compile_unary(const std::shared_ptr<AST::Unary>& unary, std::string& out) {
    if (!unary->op.empty()) {
        assert(unary->op == "-");
        assert(!"not implemented");
    }
    // we know its a primary since it's only a unary if there was a '-', which is not implemented
    if (auto primary = dynamic_cast<AST::Primary*>(unary->unary_or_primary.get())) {
        if (auto numeric_literal = dynamic_cast<AST::NumericLiteral*>(primary->value.get())) {
            out = std::to_string(numeric_literal->value);
        } else if (auto string_literal = dynamic_cast<AST::StringLiteral*>(primary->value.get())) {
            // TODO: escape newlines, etc.
            std::string final_string;
            for (size_t i = 0; i < string_literal->value.size(); ++i) {
                if (string_literal->value[i] == '\\' && i + 1 < string_literal->value.size()) {
                    char c = string_literal->value[i + 1];
                    switch (c) {
                    case 'n':
                        final_string += "', 0xa, '";
                        break;
                    case '\\':
                        final_string += c;
                        break;
                    default:
                        lk::log::info() << "warning: unhandled escaped string '" + std::to_string(c) + "'.";
                        break;
                    }
                    ++i;
                } else if (string_literal->value[i] == '\'') {
                    final_string += "', 0x27, '";
                } else {
                    final_string += string_literal->value[i];
                }
            }
            auto identifier = "__str_" + std::to_string(m_asm_data.size() / 2);
            m_asm_data.push_back(tab() + identifier + "_size: dq " + std::to_string(string_literal->value.size()));
            m_asm_data.push_back(tab() + identifier + ": db '" + final_string + "', 0x0");
            out = identifier;
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

void Object::add_comment(const std::string& comment, bool do_indent) {
    std::string line;
    if (do_indent) {
        line += tab();
    }
    line += "; " + comment;
    m_asm_text.push_back(line);
}

void Object::add_newline() {
    m_asm_text.push_back("");
}

void Object::add_label(const std::string& label) {
    m_asm_text.push_back(label + ":");
}

void Object::add_instr(const std::string& instr) {
    m_asm_text.push_back(tab() + instr);
}

void Object::add_instr_ret(const std::string& from) {
    add_comment("return from " + from);
    m_asm_text.push_back(tab() + "ret");
}

void Object::add_instr_mov(const std::string& to, const std::string& from) {
    std::string real_to = to;
    std::string real_from = from;
    int i = 0;
    if (to.substr(0, 3) == "rbp") {
        real_to = "qword [" + real_to + "]";
        ++i;
    }
    if (from.substr(0, 3) == "rbp") {
        real_from = "qword [" + real_from + "]";
        ++i;
    }
    if (i > 1) {
        // we cannot have `mov <mem>, <mem>` so we need to use two instructions
        add_comment(from + " -> rax -> " + to);
        add_instr("push rax");
        add_instr_mov("rax", real_from);
        real_from = "rax";
    }
    m_asm_text.push_back(tab() + "mov " + real_to + ", " + real_from);
    if (i > 1) {
        add_instr("pop rax");
    }
}

void Object::add_instr_cmp(const std::string& a, const std::string& b) {
    std::string real_a = a;
    std::string real_b = b;
    if (a.substr(0, 3) == "rbp") {
        real_a = "qword [" + real_a + "]";
    }
    if (b.substr(0, 3) == "rbp") {
        real_b = "qword [" + real_b + "]";
    }
    add_instr("cmp " + real_a + ", " + real_b);
}

// TODO: this needs to be a function that gets called by add and mov, since they're the same.
void Object::add_instr_add(const std::string& to, const std::string& from) {
    std::string real_to = to;
    std::string real_from = from;
    if (to.substr(0, 3) == "rbp") {
        real_to = "qword [" + real_to + "]";
    }
    if (from.substr(0, 3) == "rbp") {
        real_from = "qword [" + real_from + "]";
    }
    m_asm_text.push_back(tab() + "add " + real_to + ", " + real_from);
}

void Object::add_instr_sub(const std::string& a, const std::string& b) {
    std::string real_a = a;
    std::string real_b = b;
    if (a.substr(0, 3) == "rbp") {
        real_a = "qword [" + real_a + "]";
    }
    if (b.substr(0, 3) == "rbp") {
        real_b = "qword [" + real_b + "]";
    }
    m_asm_text.push_back(tab() + "sub " + real_a + ", " + real_b);
}

void Object::add_instr_mul(const std::string& a, const std::string& b) {
    std::string real_a = a;
    std::string real_b = b;
    if (a.substr(0, 3) == "rbp") {
        real_a = "qword [" + real_a + "]";
    }
    if (b.substr(0, 3) == "rbp") {
        real_b = "qword [" + real_b + "]";
    }
    m_asm_text.push_back(tab() + "imul " + real_a + ", " + real_b);
}

void Object::add_instr_lea(const std::string& to, const std::string& operation) {
    m_asm_text.push_back(tab() + "lea " + to + ", " + operation);
}

void Object::add_instr_call(const std::string& label) {
    m_asm_text.push_back(tab() + "call " + label);
}

void Object::add_push_callee_saved_registers() {
}

void Object::add_pop_callee_saved_registers() {
}

void Object::error(const std::string& what) {
    lk::log::error() << "compiler: " << what << "\n";
}
