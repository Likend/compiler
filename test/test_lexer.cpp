#include <string>
#include <string_view>
#include <tuple>

#include <gtest/gtest.h>

#include "gtest/gtest.h"
#include "lexer.hpp"
#include "token.hpp"

class LexerTest : public testing::Test {};

using InTuple = std::tuple<std::string, Token::Type>;

class LexerTestSingle : public LexerTest,
                        public testing::WithParamInterface<InTuple> {};

TEST_P(LexerTestSingle, LexerTestSingle) {
    auto [src, expected_type] = GetParam();
    Lexer lexer{src};
    auto it = lexer.begin();
    Token token = *it;
    EXPECT_EQ(token.type, expected_type);
    ++it;
    token = *it;
    EXPECT_EQ(token.type, Token::Type::NONE);
}

template <Token::Type type, typename... T>
testing::internal::ValueArray<std::tuple<T, Token::Type>...> SingleInputs(
    T... v) {
    return testing::internal::ValueArray<std::tuple<T, Token::Type>...>(
        std::tuple<T, Token::Type>{v, type}...);
}

INSTANTIATE_TEST_CASE_P(LexerTestSingleIdenfr, LexerTestSingle,
                        SingleInputs<Token::Type::IDENFR>(
                            "a", "Z", "_", "ab", "a1", "a_", "helloWorld",
                            "x123", "_a", "_1", "__", "_var", "_123", "a_b",
                            "my_var_1", "temp123", "user2Name", "data_2024",
                            "this_is_a_very_long_identifier_123",
                            "abc123def456ghi", "init"));

INSTANTIATE_TEST_CASE_P(LexerTestSingleIntcon, LexerTestSingle,
                        SingleInputs<Token::Type::INTCON>(
                            "0", "1", "9", "10", "123", "1000", "99999",
                            "123456789", "1", "5", "9", "12", "987", "100", "1",
                            "2", "3", "4", "5", "6", "7", "8", "9"));

INSTANTIATE_TEST_CASE_P(
    LexerTestSingleStrcon, LexerTestSingle,
    SingleInputs<Token::Type::STRCON>(
        "\"\"", "\"Hello World\"", "\"12345\"", "\"!@#$%^&*()\"",
        "\"Line1\\nLine2\"", "\"%d\"", "\"Score: %d points\"",
        "\"Name: %d, Age: %d\"", "\"Mixed %d and normal text\"", "\"\\n\"",
        "\"Multiple\\nLines\\nHere\"", "\"Tab\\tNotAllowed\"",
        "\"Backslash\\\\NotAllowed\"", "\"Space After\\n %d\"",
        "\"%d at start\"", "\"at end %d\"", "\"Multiple %d %d %d\"",
        "\"(parentheses){braces}[brackets]\"", "\"a\"", "\"A\"", "\"z\"",
        "\"Z\"", "\"0\"", "\"9\"", "\" \"", "\"!\"", "\"~\""));

#define HANDLE_RESERVED_KEYWORD(X, Y) InTuple{Y, Token::Type::X},
INSTANTIATE_TEST_CASE_P(LexerTestSingleKeywords, LexerTestSingle,
                        testing::Values(EXPAND_RESERVED_KEYWORDS InTuple{
                            "", Token::Type::NONE}));
#undef HANDLE_RESERVED_KEYWORD

#define HANDLE_DELIMITER_KEYWORDS(X, Y) InTuple{Y, Token::Type::X},
INSTANTIATE_TEST_CASE_P(LexerTestSingleDelimiter, LexerTestSingle,
                        testing::Values(EXPAND_DELIMITER_KEYWORDS InTuple{
                            "", Token::Type::NONE}));
#undef HANDLE_DELIMITER_KEYWORDS

// class LexerTestIntconMultipleZero
//     : public LexerTest,
//       public testing::WithParamInterface<std::tuple<size_t,
//       std::string_view>> {
// };

// TEST_P(LexerTestIntconMultipleZero, LexerTestIntconMultipleZero) {
//     using namespace std::string_view_literals;
//     auto [count, src] = GetParam();
//     Lexer lexer{src};
//     auto it = lexer.begin();
//     for (size_t i = 0; i < count; i++) {
//         Token token = *it;
//         ++it;
//         EXPECT_EQ(token.type, Token::Type::INTCON);
//         EXPECT_EQ(token.content, "0"sv);
//     }
// }

// INSTANTIATE_TEST_CASE_P(
//     LexerTestIntconMultipleZero, LexerTestIntconMultipleZero,
//     testing::Values(std::tuple<size_t, std::string_view>{2, "007"},
//                     std::tuple<size_t, std::string_view>{1, "01234"},
//                     std::tuple<size_t, std::string_view>{3, "000"}));

class LexerTestComment : public LexerTest,
                         public testing::WithParamInterface<std::string_view> {
};

TEST_P(LexerTestComment, SingleComment) {
    Lexer lexer{GetParam()};
    auto it = lexer.begin();
    Token token = *it;
    EXPECT_EQ(token.type, Token::Type::NONE);
}

TEST_P(LexerTestComment, CommentAndInt) {
    Lexer lexer{std::string(GetParam()).append("\nint\n")};
    auto it = lexer.begin();
    Token token = *it;
    EXPECT_EQ(token.type, Token::Type::INTTK);
}

INSTANTIATE_TEST_CASE_P(
    LexerTestComment, LexerTestComment,
    testing::Values("// single line comment", "//", "// 123 numbers //",
                    "// special characters !@#$%^&*()",
                    "/* multi line comment */", "/* */",
                    "/* multi\nline\ncomment */", "/* /* nested start */",
                    "// line1\n// line2", "/* comment with // inside */",
                    "// comment with /* inside", "/*/ tricky case */",
                    "// Unicode 测试", "/* 多行\n中文\n注释 */",
                    "// last line no newline", "/*****/"));

class LexerTestError : public LexerTest,
                       public testing::WithParamInterface<std::string_view> {};

TEST_P(LexerTestError, Error) {
    Lexer lexer{GetParam()};
    bool has_error = false;
    for (auto tok : lexer) {
        if (tok.type == Token::Type::ERROR) {
            has_error = true;
            break;
        }
    }
    EXPECT_TRUE(has_error);
}

INSTANTIATE_TEST_CASE_P(LexerTestErrorComment, LexerTestError,
                        testing::Values("/* no end", "/* no end\n"));

INSTANTIATE_TEST_CASE_P(LexerTestErrorString, LexerTestError,
                        testing::Values("\"hello"));