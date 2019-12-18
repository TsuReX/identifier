cmake_minimum_required(VERSION 3.10)

set(PROJ_NAME identifier)

set(CMAKE_C_COMPILER_WORKS 1)

project(${PROJ_NAME} C)

set( CMAKE_EXECUTABLE_SUFFIX .elf )

set(C_SRC	"src/main.c")

include_directories("include")

add_definitions("-DPID_FILE=\"${PROJ_NAME}.pid\"")

add_executable(${PROJ_NAME} ${C_SRC})

target_link_libraries(${PROJ_NAME})