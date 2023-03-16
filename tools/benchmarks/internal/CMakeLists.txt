# libmem bench framework

add_executable(libmem_bench_fw ${CMAKE_CURRENT_SOURCE_DIR}/libmem_bench_fw.c)

#copy source files to build/test directory
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lbm.py DESTINATION ${CMAKE_BINARY_DIR}/tools/benchmarks/internal/)
