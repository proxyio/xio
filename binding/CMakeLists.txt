cmake_minimum_required(VERSION 2.6)
project( proxyio )

## lua
aux_source_directory( lua LUAXIO )
add_library( luaxio STATIC ${LUAXIO} )
target_link_libraries( luaxio xio )

## python
aux_source_directory( python PYXIO )
add_library( pyxio STATIC ${PYXIO} )
target_link_libraries( pyxio xio )

## compile unit_test
aux_source_directory( test/ BTEST )
aux_source_directory( lua/test BTEST )
add_executable( btest ${BTEST} )
target_link_libraries( btest xio luaxio gtest pthread profiler lua )
