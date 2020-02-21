cmake_minimum_required(VERSION 3.10)

file(GLOB CORES_SRC *.c)
find_file(AVRGCC_H xmega.h NAMES iotn85.h 
          HINTS /usr/local/opt/avr-gcc /usr/local/avr-gcc
          PATH_SUFFIXES avr/include/avr avr )
Message("AVRGCC:" ${AVRGCC_H})

get_filename_component(AVRGCC_INCLUDE_DIR ${AVRGCC_H} DIRECTORY)

add_library(cores OBJECT EXCLUDE_FROM_ALL ${CORES_SRC})
target_include_directories(cores PRIVATE ./avr ../sim ${AVRGCC_INCLUDE_DIR}/../)