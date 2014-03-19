cmake_minimum_required(VERSION 2.6)
project( proxyio )

include_directories(
	${PROJECT_SOURCE_DIR}/src/
	${PROJECT_SOURCE_DIR}/src/utils/
)

link_directories(
	${PROJECT_BUILD_DIR}/lib/
)

## this setting must place before add_subdirectory ...
set(CMAKE_VERBOSE_MAKEFILE off)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

## debug setting
set(DEBUG_mode off)
if (DEBUG_mode)
   add_definitions(-fprofile-arcs -ftest-coverage)
   message(STATUS "Debug version")
elseif (NOT DEBUG_mode)
   message(STATUS "Release version")
endif()
set( CMAKE_C_FLAGS "-O0 -g -ggdb -Wall -DTRACE_TIME" )
set( CMAKE_CXX_FLAGS "-O0 -g -ggdb -Wall -DTRACE_TIME" )

enable_testing()
add_subdirectory(src)

## include( src/CMakeTest.cmake )
