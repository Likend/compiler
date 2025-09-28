#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

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
        Lexer lexer{*src};
        std::vector<Token> tokens;
        bool error_flag = false;
        for (const Token& token : lexer) {
            // std::cout << token.type << ' ' << token.content << std::endl;
            tokens.push_back(token);
            if (token.type == Token::Type::ERROR) {
                error_flag = true;
            }
        }
        if (error_flag) {
            std::cout << "2 a" << std::endl;
        } else {
            for (const Token& token : tokens) {
                std::cout << token.type << ' ' << token.content << std::endl;
            }
        }
    } else
        std::cout << "Cannot open file!" << std::endl;

    return 0;
}