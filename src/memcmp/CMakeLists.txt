
set(SRC_PATH  ${CMAKE_CURRENT_SOURCE_DIR})

if (ENABLE_TUNABLES)
set(SOURCE_FILES ${SRC_PATH}/memcmp_avx2.c ${SRC_PATH}/memcmp_erms.c  ${SRC_PATH}/memcmp_system.c ${SRC_PATH}/memcmp_threshold.c ${SRC_PATH}/memcmp_march.c)
else ()
set(SOURCE_FILES ${SRC_PATH}/amd_memcmp.c)
endif ()

target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})
