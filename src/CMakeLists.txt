cmake_minimum_required(VERSION 2.6)
project( proxyio )

## compile common
aux_source_directory( common/ds COMMON )
aux_source_directory( common/hash COMMON )
aux_source_directory( common/net COMMON )
aux_source_directory( common/os COMMON )
aux_source_directory( common/path COMMON )
aux_source_directory( common/runner COMMON )
aux_source_directory( common/stats COMMON )
aux_source_directory( common/sync COMMON )
aux_source_directory( common/proto COMMON )
add_library( common STATIC ${COMMON} )

## compile common_test
aux_source_directory( common/test COMMON_TEST )
add_executable( common_test ${COMMON_TEST} )
target_link_libraries( common_test common gtest pthread)
if (DEBUG_mode)
   target_link_libraries( common_test gcov )
endif()
