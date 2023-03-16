# bench framework

add_subdirectory(internal)
add_subdirectory(external)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Benchmark_Framework.md
DESTINATION ${CMAKE_BINARY_DIR}/tools/benchmarks/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/bench.py
DESTINATION ${CMAKE_BINARY_DIR}/test/)
