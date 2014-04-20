cmake_minimum_required(VERSION 2.6)
project( proxyio )

## compile base
aux_source_directory( base/ BASE )
aux_source_directory( base/bufio BASE )
aux_source_directory( base/ds BASE )
aux_source_directory( base/hash BASE )
aux_source_directory( base/transport BASE )
aux_source_directory( base/transport/tcp BASE )
aux_source_directory( base/transport/ipc BASE )
aux_source_directory( base/os BASE )
aux_source_directory( base/syscall BASE )
aux_source_directory( base/path BASE )
aux_source_directory( base/runner BASE )
aux_source_directory( base/stats BASE )
aux_source_directory( base/sync BASE )
add_library( base STATIC ${BASE} )


## compile x
aux_source_directory( x X )
add_library( x STATIC ${X} )
target_link_libraries( x base uuid )


## compile ep
## aux_source_directory( ep EP )
## add_library( ep STATIC ${EP} )
## target_link_libraries( ep base x )



## compile unit_test
aux_source_directory( test UNIT_TEST )
aux_source_directory( base/test UNIT_TEST )
## aux_source_directory( ep/test UNIT_TEST )
aux_source_directory( x/test UNIT_TEST )
add_executable( unit_test ${UNIT_TEST} )
## target_link_libraries( unit_test base x ep gtest pthread)
target_link_libraries( unit_test base x gtest pthread)
if (DEBUG_mode)
   target_link_libraries( unit_test gcov )
endif()


## compile sdk for c/c++
aux_source_directory( sdk/c CSDK )
##add_library( pioc ${CSDK} )
##target_link_libraries( pioc base core )



## compile pio daemon
aux_source_directory( service/piod PIOD )
## add_executable( piod ${PIOD} )
## target_link_libraries( piod base core pioc)
##if (DEBUG_mode)
##   target_link_libraries( piod gcov profiler)
##endif()


## compile pio benchmark
aux_source_directory( service/benchmark PIOBENCHMARK )
##add_executable( pio_benchmark ${PIOBENCHMARK} )
##target_link_libraries( pio_benchmark base pioc pthread uuid)
##if (DEBUG_mode)
##   target_link_libraries( pio_benchmark gcov )
##endif()
