#cmake for memcpy
set(SRC_PATH  ${CMAKE_CURRENT_SOURCE_DIR})

if (ENABLE_TUNABLES)
set(SOURCE_FILES  ${SRC_PATH}/memcpy_march.c ${SRC_PATH}/memcpy_avx2.c)
else ()
set(SOURCE_FILES ${SRC_PATH}/amd_memcpy.c)
endif ()
target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})
