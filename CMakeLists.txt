cmake_minimum_required(VERSION 3.16) # c++20 support required
project(compiler)

set(CMAKE_CXX_STANDARD 20)

set(LK_SCRATCHY_STATIC ON)
add_subdirectory(deps/LK)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra --pedantic -fno-exceptions")

add_executable(compiler
    src/main.cpp
    src/ASTParser.h src/ASTParser.cpp
    src/Common.h
    )

target_link_libraries(compiler LK)
target_include_directories(compiler PRIVATE deps/LK/src)
