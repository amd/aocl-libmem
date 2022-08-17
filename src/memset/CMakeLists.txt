
set(SRC_PATH  ${CMAKE_CURRENT_SOURCE_DIR})

if (ENABLE_TUNABLES)
set(SOURCE_FILES ${SRC_PATH}/memset_avx2.c ${SRC_PATH}/memset_erms.c  ${SRC_PATH}/memset_system.c ${SRC_PATH}/memset_threshold.c ${SRC_PATH}/memset_march.c)
else ()
set(SOURCE_FILES ${SRC_PATH}/amd_memset.c)
endif ()

target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})
