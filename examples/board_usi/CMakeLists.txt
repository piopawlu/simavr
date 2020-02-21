cmake_minimum_required(VERSION 3.10)

add_executable(usi EXCLUDE_FROM_ALL usi.c)

target_link_libraries(usi PRIVATE simavr)
