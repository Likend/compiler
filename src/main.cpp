#include <algorithm>
#include <fstream>
#include <ios>
#include <iostream>
#include <optional>
#include <string>

#ifndef DEBUG_TOKEN_TYPE_NAME
#define DEBUG_TOKEN_TYPE_NAME
#endif
#include "error.hpp"
#include "grammer.hpp"
#include "lexer.hpp"
#include "symbol_table.hpp"
#include "token.hpp"
#include "util/assert.hpp"
#include "visitor.hpp"

std::optional<std::string> read_file() {
    std::ifstream file{"./testfile.txt"};
    if (file)
        return std::string{std::istreambuf_iterator<char>(file),
                           std::istreambuf_iterator<char>()};
    else
        return std::nullopt;
}

void print_error_infos() {
    if (error_infos.size() == 0) return;
    std::cout << "Error count: " << error_infos.size() << std::endl;
    std::stable_sort(error_infos.begin(), error_infos.end(),
                     [](const ErrorInfo& a, const ErrorInfo& b) {
                         if (a.token.line < b.token.line) return true;
                         return a.token.col < b.token.col;
                     });
    std::ofstream output{"./error.txt", std::ios_base::out};
    for (auto& error : error_infos) {
#ifdef NDEBUG
        output << error.line << ' ' << error.type << std::endl;
#else
        output << error.token.line << ':' << error.token.col << ' '
               << error.token.type << ' ' << error.token.content << ' '
               << static_cast<char>(error.type) << ' ' << error.msg
               << std::endl;
#endif
    }
}

void print_ast(const ASTNode& ast) {
    std::ofstream output{"./parser.txt", std::ios_base::out};
    output << ast;
}

void print_symbol_record(std::vector<SymbolRecord> records) {
    std::ofstream output{"./symbol.txt", std::ios_base::out};
    std::stable_sort(records.begin(), records.end(),
                     [](const SymbolRecord& a, const SymbolRecord& b) {
                         return a.scope_index < b.scope_index;
                     });
    for (const SymbolRecord& record : records) {
        output << record.scope_index << ' ' << record.ident_name << ' '
               << record.attr.type << std::endl;
    }
}

int main() {
    if (std::optional<std::string> src = read_file()) {
        Lexer lexer{*src};
        auto it = lexer.begin();
        auto map = parse_grammer(it);
        const auto* const ast = map->get(ASTNode::Type::COMP_UNIT);

        if (ast) {
            print_ast(*ast);

            Visitor visitor{*ast};
            visitor.write_ir("ir.ll");
            print_symbol_record(visitor.records);
        }
        print_error_infos();

        if (*it) {
            std::cerr << "Unexpected Token: " << (*it).type << " in "
                      << (*it).line << ':' << (*it).col << std::endl;
        }
    } else
        std::cout << "Cannot open file!" << std::endl;

    return 0;
}
