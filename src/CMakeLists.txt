cmake_minimum_required(VERSION 2.6)
project( proxyio )

## compile common
aux_source_directory( common/bufio COMMON )
aux_source_directory( common/ds COMMON )
aux_source_directory( common/hash COMMON )
aux_source_directory( common/net COMMON )
aux_source_directory( common/os COMMON )
aux_source_directory( common/path COMMON )
aux_source_directory( common/runner COMMON )
aux_source_directory( common/stats COMMON )
aux_source_directory( common/sync COMMON )
add_library( common STATIC ${COMMON} )

## compile common_test
aux_source_directory( common/test COMMON_TEST )
add_executable( common_test ${COMMON_TEST} )
target_link_libraries( common_test common gtest pthread)
if (DEBUG_mode)
   target_link_libraries( common_test gcov )
endif()


## compile core
aux_source_directory( core CORE )
add_library( core STATIC ${CORE} )
target_link_libraries( core common gtest uuid pthread)

## compile core_test
aux_source_directory( core/test CORE_TEST )
add_executable( core_test ${CORE_TEST} )
target_link_libraries( core_test core pioc)
if (DEBUG_mode)
   target_link_libraries( core_test gcov )
endif()


## compile sdk for c/c++
aux_source_directory( sdk/c CSDK )
add_library( pioc ${CSDK} )
target_link_libraries( common_test common)
