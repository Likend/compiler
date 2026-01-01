#include <algorithm>
#include <fstream>
#include <ios>
#include <iostream>
#include <optional>
#include <string>

#include "codegen/FillFrame.hpp"
#include "codegen/IRTranslator.hpp"
#include "codegen/LinerScanRegisterAlloc.hpp"
#include "codegen/MachineModule.hpp"
#include "codegen/MipsPrinter.hpp"
#include "codegen/MIRPrinter.hpp"
#include "codegen/Register.hpp"
#include "codegen/ReplaceRegister.hpp"
#include "ir/IRPrinter.hpp"
#include "ir/Pass.hpp"
#include "opt/SimplifyCFG.hpp"

#ifndef DEBUG_TOKEN_TYPE_NAME
#define DEBUG_TOKEN_TYPE_NAME
#endif
#include "error.hpp"
#include "grammer.hpp"
#include "lexer.hpp"
#include "symbol_table.hpp"
#include "token.hpp"
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
        output << error.token.line << ' ' << static_cast<char>(error.type)
               << std::endl;
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
    using namespace codegen;

    if (std::optional<std::string> src = read_file()) {
        Lexer lexer{*src};
        auto  it  = lexer.begin();
        auto  map = parse_grammer(it);

        const auto* const ast = map->get(ASTNode::Type::COMP_UNIT);

        if (ast) {
            print_ast(*ast);

            Visitor visitor{*ast};
            print_symbol_record(visitor.records);

            std::ofstream   ir1_file{"llvm_ir.txt", std::ios_base::out};
            std::ofstream   ir2_file{"llvm_ir2.txt", std::ios_base::out};
            std::ofstream   mir_file{"mir.txt", std::ios_base::out};
            std::ofstream   mir1_file{"mir1.txt", std::ios_base::out};
            std::ofstream   mips_file{"mips.txt", std::ios_base::out};
            ir::PassManager pm{
                new ir::IRPrinterPass{ir1_file},
                new opt::SimplifyCFGPass{},
                new ir::IRPrinterPass{ir2_file},
                new codegen::MachineModuleAnalysisPass{},
                new codegen::IRTranslator{},
                new codegen::MIRPrinterPass{mir_file},
                new codegen::LinerScanRegisterAllocPass{
                    REG_T0, REG_T1, REG_T2, REG_T3, REG_T4, REG_T5, REG_T6,
                    REG_T7, REG_S0, REG_S1, REG_S2, REG_S3, REG_S4, REG_S5,
                    REG_S6, REG_S7},
                new codegen::ReplaceRegisterPass{REG_T8, REG_T9},
                new codegen::MIRPrinterPass{mir1_file},
                new codegen::FillFramePass{},
                new codegen::MipsPrinterPass{mips_file},
            };
            visitor.runPass(pm);
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
