#cmake for memcpy
set(SRC_PATH  ${CMAKE_CURRENT_SOURCE_DIR})

if (ENABLE_TUNABLES)
    set(SOURCE_FILES  ${SRC_PATH}/memcpy_march.c ${SRC_PATH}/memcpy_avx2.c ${SRC_PATH}/memcpy_erms.c ${SRC_PATH}/memcpy_system ${SRC_PATH}/memcpy_threshold)
    if (DEFINED AVX512_FEATURE_AVAILABLE)
        set(SOURCE_FILES ${SOURCE_FILES} ${SRC_PATH}/memcpy_avx512.c)
    endif ()
else ()
    set(SOURCE_FILES ${SRC_PATH}/amd_memcpy.c)
endif ()

target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})
