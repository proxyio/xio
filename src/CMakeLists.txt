cmake_minimum_required(VERSION 2.6)
project( proxyio )

## compile utils
aux_source_directory( utils/bufio UTILS )
aux_source_directory( utils/ds UTILS )
aux_source_directory( utils/hash UTILS )
aux_source_directory( utils/net UTILS )
aux_source_directory( utils/os UTILS )
aux_source_directory( utils/path UTILS )
aux_source_directory( utils/runner UTILS )
aux_source_directory( utils/stats UTILS )
aux_source_directory( utils/sync UTILS )
add_library( utils STATIC ${UTILS} )

## compile utils_test
aux_source_directory( utils/test UTILS_TEST )
add_executable( utils_test ${UTILS_TEST} )
target_link_libraries( utils_test utils gtest pthread)
if (DEBUG_mode)
   target_link_libraries( utils_test gcov )
endif()


## compile core
aux_source_directory( core CORE )
add_library( core STATIC ${CORE} )
target_link_libraries( core utils uuid pthread)

## compile core_test
aux_source_directory( core/test CORE_TEST )
add_executable( core_test ${CORE_TEST} )
target_link_libraries( core_test core pioc gtest )
if (DEBUG_mode)
   target_link_libraries( core_test gcov )
endif()


## compile sdk for c/c++
aux_source_directory( sdk/c CSDK )
add_library( pioc ${CSDK} )
target_link_libraries( pioc utils core )



## compile pio daemon
aux_source_directory( service/piod PIOD )
add_executable( piod ${PIOD} )
target_link_libraries( piod utils core pioc)
if (DEBUG_mode)
   target_link_libraries( piod gcov profiler)
endif()


## compile pio benchmark
aux_source_directory( service/benchmark PIOBENCHMARK )
add_executable( pio_benchmark ${PIOBENCHMARK} )
target_link_libraries( pio_benchmark utils pioc pthread uuid)
if (DEBUG_mode)
   target_link_libraries( pio_benchmark gcov )
endif()
