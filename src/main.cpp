#include <fstream>
#include <ios>
#include <iostream>
#include <optional>
#include <string>

#ifndef DEBUG_TOKEN_TYPE_NAME
#define DEBUG_TOKEN_TYPE_NAME
#endif
#include "grammer.hpp"
#include "lexer.hpp"
#include "token.hpp"

std::optional<std::string> read_file() {
    std::ifstream file{"./testfile.txt"};
    if (file)
        return std::string{std::istreambuf_iterator<char>(file),
                           std::istreambuf_iterator<char>()};
    else
        return std::nullopt;
}

int main() {
    std::optional<std::string> src = read_file();
    if (src) {
        Lexer lexer{*src};
        auto it = lexer.begin();
        auto ast = parse_grammer(it);

        if (error_infos.size()) {
            std::cout << "Error count: " << error_infos.size() << std::endl;
            // std::sort(error_infos.begin(), error_infos.end());
            std::ofstream output{"./error.txt", std::ios_base::out};
            for (auto& error : error_infos) {
                output << error.line << ' ' << error.type << std::endl;
            }
        }
        if (ast) {
            std::ofstream output{"./parser.txt", std::ios_base::out};
            output << *ast;
        }
        if (*it) {
            std::cerr << "Unexpected Token: " << (*it).type << " in "
                      << (*it).line << ':' << (*it).col << std::endl;
        }
    } else
        std::cout << "Cannot open file!" << std::endl;

    return 0;
}