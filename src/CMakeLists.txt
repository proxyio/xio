cmake_minimum_required(VERSION 2.6)
project( proxyio )

## compile xlibc
aux_source_directory( libc/ LIBC )
aux_source_directory( libc/bufio LIBC )
aux_source_directory( libc/ds LIBC )
aux_source_directory( libc/hash LIBC )
aux_source_directory( libc/transport LIBC )
aux_source_directory( libc/transport/tcp LIBC )
aux_source_directory( libc/transport/ipc LIBC )
aux_source_directory( libc/os LIBC )
aux_source_directory( libc/syscall LIBC )
aux_source_directory( libc/path LIBC )
aux_source_directory( libc/runner LIBC )
aux_source_directory( libc/stats LIBC )
aux_source_directory( libc/sync LIBC )
add_library( xlibc STATIC ${LIBC} )


## compile socket
aux_source_directory( sock XIO )
aux_source_directory( poll XIO )
add_library( xio STATIC ${XIO} )
target_link_libraries( xio xlibc uuid )


## compile ep
## aux_source_directory( ep EP )
## add_library( ep STATIC ${EP} )
## target_link_libraries( ep xlibc xio )



## compile unit_test
aux_source_directory( tools/test UNIT_TEST )
aux_source_directory( libc/test UNIT_TEST )
## aux_source_directory( ep/test UNIT_TEST )
aux_source_directory( sock/test UNIT_TEST )
add_executable( unit_test ${UNIT_TEST} )
## target_link_libraries( unit_test xlibc xio ep gtest pthread)
target_link_libraries( unit_test xlibc xio gtest pthread )
if (DEBUG_mode)
   target_link_libraries( unit_test gcov )
endif()


## compile sdk for c/c++
aux_source_directory( sdk/c CSDK )
##add_library( pioc ${CSDK} )
##target_link_libraries( pioc xlibc core )



## compile pio daemon
aux_source_directory( service/piod PIOD )
## add_executable( piod ${PIOD} )
## target_link_libraries( piod xlibc core pioc)
##if (DEBUG_mode)
##   target_link_libraries( piod gcov profiler)
##endif()


## compile pio benchmark
aux_source_directory( service/benchmark PIOBENCHMARK )
##add_executable( pio_benchmark ${PIOBENCHMARK} )
##target_link_libraries( pio_benchmark xlibc pioc pthread uuid)
##if (DEBUG_mode)
##   target_link_libraries( pio_benchmark gcov )
##endif()
