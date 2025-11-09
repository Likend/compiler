cmake_minimum_required(VERSION 3.15)

if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

project(Compiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(Compiler_lib STATIC
    src/token.cpp
    src/grammer.cpp
    src/lexer.cpp
    src/symbol_table.cpp
    src/visitor.cpp
    src/error.cpp
)
target_compile_definitions(Compiler_lib PRIVATE DEBUG_TOKEN_TYPE_NAME)

add_executable(Compiler src/main.cpp)
target_link_libraries(Compiler Compiler_lib)

target_compile_definitions(Compiler PRIVATE DEBUG_TOKEN_TYPE_NAME)

option(BUILD_TESTING "Build unit tests" OFF) # 定义选项，默认为 OFF（不构建测试）
if(BUILD_TESTING)
    add_subdirectory(test)
endif()