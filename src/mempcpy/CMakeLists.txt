#cmake for mempcpy
set(SRC_PATH  ${CMAKE_CURRENT_SOURCE_DIR})

if (ENABLE_TUNABLES)
set(SOURCE_FILES ${SRC_PATH}/mempcpy_avx2.c ${SRC_PATH}/mempcpy_erms.c)
else ()
set(SOURCE_FILES ${SRC_PATH}/amd_mempcpy.c)
endif ()
target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})
