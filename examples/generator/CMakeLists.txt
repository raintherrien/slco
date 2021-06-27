cmake_minimum_required(VERSION 3.9)
project(generator VERSION 1 LANGUAGES C)

add_executable(generator ${PROJECT_SOURCE_DIR}/generator.c)
target_link_libraries(generator PRIVATE slco)
