# libmem source directory

add_compile_options(-Wall -Wextra -Werror -Wno-unused-function -fno-common -Wundef -fPIC -flto -march=native -fvisibility=hidden -mavx2 -O1)

# build a shared library
add_library(${PROJECT_NAME} SHARED "")

set(SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/cache_info.c   ${CMAKE_CURRENT_SOURCE_DIR}/cpu_features.c  ${CMAKE_CURRENT_SOURCE_DIR}/threshold.c  ${CMAKE_CURRENT_SOURCE_DIR}/libmem.c)

if (ENABLE_LOGGING)
set(SOURCE_FILES ${SOURCE_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/logger.c)
endif ()

# include the header files
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})

target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})

add_subdirectory(memcpy)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${CMAKE_BINARY_DIR}/lib)
