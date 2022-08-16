# libmem test framework

add_subdirectory(memcpy)
add_subdirectory(mempcpy)
add_subdirectory(memmove)

#copy source files to build directory
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/test_libmem.py DESTINATION ${CMAKE_BINARY_DIR}/test)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/memcpy/test_memcpy.c DESTINATION ${CMAKE_BINARY_DIR}/test/memcpy/)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/mempcpy/test_mempcpy.c DESTINATION ${CMAKE_BINARY_DIR}/test/mempcpy/)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/memmove/test_memmove.c DESTINATION ${CMAKE_BINARY_DIR}/test/memmove/)
