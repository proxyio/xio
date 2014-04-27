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
aux_source_directory( socket XIO )
aux_source_directory( poll XIO )
add_library( xio STATIC ${XIO} )
target_link_libraries( xio xlibc uuid )

## compile unit_test
aux_source_directory( tools/test UNIT_TEST )
aux_source_directory( libc/test UNIT_TEST )
aux_source_directory( socket/test UNIT_TEST )
add_executable( unit_test ${UNIT_TEST} )
target_link_libraries( unit_test xlibc xio gtest pthread )
if (DEBUG_mode)
   target_link_libraries( unit_test gcov )
endif()
