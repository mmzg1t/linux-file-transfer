cmake_minimum_required(VERSION 3.16)
project(file_transfer LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_compile_definitions(_POSIX_C_SOURCE=200809L)
add_compile_options(-Wall -Wextra -Wpedantic)

add_library(transfer_common STATIC common.c)
add_executable(file_server file_server.c)
add_executable(file_client file_client.c)
target_link_libraries(file_server PRIVATE transfer_common)
target_link_libraries(file_client PRIVATE transfer_common)
