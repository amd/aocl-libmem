# libmem test framework

add_subdirectory(memcpy)

#copy source files to build directory
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/test_libmem.py DESTINATION ${CMAKE_BINARY_DIR}/test)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/memcpy/test_memcpy.c DESTINATION ${CMAKE_BINARY_DIR}/test/memcpy/)
