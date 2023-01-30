add_subdirectory(gbench)
add_subdirectory(tinybench)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/graph_generator.py
DESTINATION ${CMAKE_BINARY_DIR}/test/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/bench.py
DESTINATION ${CMAKE_BINARY_DIR}/test/)

