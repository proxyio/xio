cmake_minimum_required(VERSION 2.6)
project( proxyio )

## compile xio
aux_source_directory( utils XIO )
aux_source_directory( utils/tcp XIO )
aux_source_directory( utils/ipc XIO )
aux_source_directory( socket XIO )
aux_source_directory( socket/inproc XIO )
aux_source_directory( socket/tcp XIO )
aux_source_directory( socket/mul XIO )
aux_source_directory( poll XIO )
aux_source_directory( sp XIO )
aux_source_directory( sp/reqrep XIO )
add_library( xio STATIC ${XIO} )
target_link_libraries( xio uuid profiler )

## compile unit_test
aux_source_directory( poll/test UNIT_TEST )
aux_source_directory( utils/test UNIT_TEST )
aux_source_directory( socket/test UNIT_TEST )
aux_source_directory( sp/test UNIT_TEST )
add_executable( unit_test ${UNIT_TEST} )
target_link_libraries( unit_test xio gtest pthread profiler )
if (DEBUG_mode)
   target_link_libraries( unit_test gcov )
endif()

## compile xiobc
aux_source_directory( bc/ XIOBC )
add_executable( xiobc ${XIOBC} )
target_link_libraries( xiobc xio pthread profiler )
