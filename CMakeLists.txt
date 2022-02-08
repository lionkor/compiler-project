cmake_minimum_required(VERSION 3.16) # c++20 support required
project(compiler)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra --pedantic")

add_subdirectory(deps/liblk)

add_executable(compiler
    src/main.cpp
    src/ASTParser.h src/ASTParser.cpp
    src/Common.h
    )

target_link_libraries(compiler lk)
