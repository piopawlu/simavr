cmake_minimum_required(VERSION 3.10)

file(GLOB SIM_SRC *.c)
find_library(LIBELF REQUIRED elf NAMES 'libelf')
find_file(LIBELF_H libelf.h HINTS /usr/local/opt /usr/local
          PATH_SUFFIXES include libelf include/libelf)

Message("LIBELF:" ${LIBELF_H})

get_filename_component(LIBELF_INCLUDE_DIR ${LIBELF_H} DIRECTORY)

add_library(sim OBJECT EXCLUDE_FROM_ALL ${SIM_SRC})
target_include_directories(sim PRIVATE ../ ${LIBELF_INCLUDE_DIR} ${LIBELF_INCLUDE_DIR}/../)
target_link_libraries(sim ${LIBELF})