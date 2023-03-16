add_subdirectory(gbench)
add_subdirectory(tinybench)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/graph_generator.py
DESTINATION ${CMAKE_BINARY_DIR}/test/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tbm.py
DESTINATION ${CMAKE_BINARY_DIR}/tools/benchmarks/external/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/fbm.py
DESTINATION ${CMAKE_BINARY_DIR}/tools/benchmarks/external/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/gbm.py
DESTINATION ${CMAKE_BINARY_DIR}/tools/benchmarks/external/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/GetParser.py
DESTINATION ${CMAKE_BINARY_DIR}/tools/benchmarks/external/)
