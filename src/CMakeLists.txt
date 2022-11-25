# libmem source directory

add_compile_options(-Wall -Wextra -Werror -Wno-unused-function -fno-common -Wundef -fvisibility=hidden -mavx2 -O1 -mbmi)

if (AVX512_FEATURE_AVAILABLE)
    add_compile_options(-mavx512f -mavx512bw)
endif ()

if ((${CMAKE_C_COMPILER_ID}  STREQUAL "Clang") AND ENABLE_TUNABLES)
    add_definitions(-DALMEM_CMPSD_CLANG_WORKAROUND)
endif ()

if (${BUILD_SHARED_LIBS})
# build a shared library
add_compile_options(-fPIC -flto)
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
else ()
# build a static library
add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
endif()

set(SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/cache_info.c   ${CMAKE_CURRENT_SOURCE_DIR}/cpu_features.c  ${CMAKE_CURRENT_SOURCE_DIR}/threshold.c  ${CMAKE_CURRENT_SOURCE_DIR}/libmem.c)

if (ENABLE_LOGGING)
set(SOURCE_FILES ${SOURCE_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/logger.c)
endif ()

if (ENABLE_TUNABLES)
add_definitions(-DENABLE_TUNABLES)
set(SOURCE_FILES ${SOURCE_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/env_parser.c  ${CMAKE_CURRENT_SOURCE_DIR}/libmem_iface.c)
endif ()

# include the header files
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/include)

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})

target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})

add_subdirectory(memcpy)
add_subdirectory(mempcpy)
add_subdirectory(memmove)
add_subdirectory(memset)
add_subdirectory(memcmp)

if (${BUILD_SHARED_LIBS})
install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${CMAKE_BINARY_DIR}/lib/shared)
else ()
install(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION ${CMAKE_BINARY_DIR}/lib/static)
endif ()
