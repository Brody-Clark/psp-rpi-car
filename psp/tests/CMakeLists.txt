cmake_minimum_required(VERSION 3.11)
project(psp_protocol_tests C)

enable_testing()

add_executable(test_protocol test_protocol.c)
target_include_directories(test_protocol PRIVATE ../include)
target_compile_options(test_protocol PRIVATE -Wall -Wextra -Werror)

add_test(NAME psp_protocol COMMAND test_protocol)
