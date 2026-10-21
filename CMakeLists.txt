
cmake_minimum_required(VERSION 3.20)

project(TerminalMessenger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(server main.cpp)
add_executable(client client.cpp)