cmake_minimum_required(VERSION 3.16)
project(slc)

set_property( GLOBAL PROPERTY USE_FOLDERS ON)

include_directories(
	${CMAKE_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/datastr
	${CMAKE_SOURCE_DIR}/utils
)

enable_testing()
find_package(GTest CONFIG REQUIRED)

if(WIN32)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
add_definitions(-DDEV)
add_definitions(-DCODE_FILE)
endif(WIN32)

add_library(lexer STATIC lexer.c lexer.h)
add_library(dev STATIC dev.c dev.h)
add_library(parser STATIC parser.c parser.h)
add_library(codegen STATIC codegen.c codegen.h)
add_executable(slc main.c)
target_link_libraries(slc codegen dev lexer parser utils datastr)

add_subdirectory(datastr)
add_subdirectory(utils)
add_subdirectory(unit_tests)
