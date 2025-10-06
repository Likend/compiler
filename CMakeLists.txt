cmake_minimum_required(VERSION 3.28)

if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

project(Compiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


file(GLOB_RECURSE SRC_FILES src/*.cpp)

add_executable(Compiler ${SRC_FILES})

target_compile_definitions(Compiler PRIVATE DEBUG_TOKEN_TYPE_NAME)