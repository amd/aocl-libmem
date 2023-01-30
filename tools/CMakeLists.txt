# libmem tools

add_subdirectory(benchmarks)
add_subdirectory(analyser)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/cpufreq.py
DESTINATION ${CMAKE_BINARY_DIR}/test/)

