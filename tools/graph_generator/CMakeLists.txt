# libmem test framework
#copy source files to build directory
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/histogram.py DESTINATION ${CMAKE_BINARY_DIR}/tools/graph_generator)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/linechart.py DESTINATION ${CMAKE_BINARY_DIR}/tools/graph_generator)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/graph_generator.py
DESTINATION ${CMAKE_BINARY_DIR}/tools/graph_generator)

