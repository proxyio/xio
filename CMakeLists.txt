cmake_minimum_required(VERSION 2.6)
project( proxyio )

include_directories(
	${PROJECT_SOURCE_DIR}/binding/
	${PROJECT_SOURCE_DIR}/src/
	${PROJECT_SOURCE_DIR}/src/libc/
	${PROJECT_SOURCE_DIR}/include/
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
add_subdirectory(binding)

## include( src/CMakeTest.cmake )

add_test(test ${PROJECT_BINARY_DIR}/bin/unit_test)
add_test(test ${PROJECT_BINARY_DIR}/bin/btest)

install( DIRECTORY ${PROJECT_SOURCE_DIR}/include/xio DESTINATION /usr/local/include )
install( PROGRAMS ${PROJECT_BINARY_DIR}/lib/libxio.a DESTINATION /usr/local/lib/ )


