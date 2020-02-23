cmake_minimum_required(VERSION 3.10)

add_subdirectory(sim EXCLUDE_FROM_ALL)
add_subdirectory(cores EXCLUDE_FROM_ALL)

if( NOT TARGET simavr)
  add_library(simavr STATIC $<TARGET_OBJECTS:sim> $<TARGET_OBJECTS:cores> sim_core_config.c)
  target_include_directories(simavr PUBLIC sim cores)
  target_link_libraries(simavr PRIVATE sim cores)
endif()

if( NOT TARGET simavr-cli)
  add_executable(simavr-cli EXCLUDE_FROM_ALL run_avr.c)
  add_dependencies(simavr-cli simavr)
  target_include_directories(simavr-cli PRIVATE sim cores)
  target_link_libraries(simavr-cli PRIVATE simavr)
endif()
