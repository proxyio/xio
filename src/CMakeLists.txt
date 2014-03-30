cmake_minimum_required(VERSION 2.6)
project( proxyio )

## compile base
aux_source_directory( base/ BASE )
aux_source_directory( base/bufio BASE )
aux_source_directory( base/ds BASE )
aux_source_directory( base/hash BASE )
aux_source_directory( base/channel BASE )
aux_source_directory( base/transport BASE )
aux_source_directory( base/transport/tcp BASE )
aux_source_directory( base/transport/ipc BASE )
aux_source_directory( base/os BASE )
aux_source_directory( base/path BASE )
aux_source_directory( base/runner BASE )
aux_source_directory( base/stats BASE )
aux_source_directory( base/sync BASE )
add_library( base STATIC ${BASE} )

## compile base_test
aux_source_directory( base/test BASE_TEST )
add_executable( base_test ${BASE_TEST} )
target_link_libraries( base_test base gtest pthread)
if (DEBUG_mode)
   target_link_libraries( base_test gcov )
endif()


## compile core
aux_source_directory( core CORE )
add_library( core STATIC ${CORE} )
target_link_libraries( core base uuid pthread)

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
target_link_libraries( pioc base core )



## compile pio daemon
aux_source_directory( service/piod PIOD )
add_executable( piod ${PIOD} )
target_link_libraries( piod base core pioc)
if (DEBUG_mode)
   target_link_libraries( piod gcov profiler)
endif()


## compile pio benchmark
aux_source_directory( service/benchmark PIOBENCHMARK )
add_executable( pio_benchmark ${PIOBENCHMARK} )
target_link_libraries( pio_benchmark base pioc pthread uuid)
if (DEBUG_mode)
   target_link_libraries( pio_benchmark gcov )
endif()
