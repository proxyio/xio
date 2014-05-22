cmake_minimum_required(VERSION 2.6)
project( proxyio )

## lua51
aux_source_directory( lua51 LUA51 )
add_library( luaxio STATIC ${LUA51} )
target_link_libraries( luaxio xio )

## compile unit_test
aux_source_directory( test/ BTEST )
aux_source_directory( lua51/test BTEST )
add_executable( btest ${BTEST} )
target_link_libraries( btest xio luaxio gtest pthread profiler lua )
