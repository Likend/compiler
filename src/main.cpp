#include <fstream>
#include <ios>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#ifndef DEBUG_TOKEN_TYPE_NAME
#define DEBUG_TOKEN_TYPE_NAME
#endif
#include "lexer.hpp"
#include "token.hpp"

std::optional<std::string> read_file() {
    std::ifstream file{"./testfile.txt"};
    if (file)
        return std::string{(std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>()};
    else
        return std::nullopt;
}

int main() {
    std::optional<std::string> src = read_file();
    if (src) {
        // std::cout << *src << std::endl;
        // src->push_back('\0');
        Lexer lexer{*src};
        std::vector<Token> tokens;
        Token error_tok;
        for (const Token& token : lexer) {
            std::cout << token.type << ' ' << token.line << ':' << token.col
                      << ' ' << token.content << std::endl;
            if (token.type != Token::Type::COMMENT &&
                token.type != Token::Type::WHITESPACE)
                tokens.push_back(token);
            if (token.type == Token::Type::ERROR) {
                error_tok = token;
                break;
            }
        }
        if (error_tok) {
            std::ofstream output{"./error.txt", std::ios_base::out};
            output << error_tok.line << ' ' << 'a' << std::endl;
        } else {
            std::ofstream output{"./lexer.txt", std::ios_base::out};
            for (const Token& token : tokens) {
                output << token.type << ' ' << token.content << std::endl;
            }
        }
    } else
        std::cout << "Cannot open file!" << std::endl;

    return 0;
}