
set(SRC_PATH  ${CMAKE_CURRENT_SOURCE_DIR})

if (ENABLE_TUNABLES)
set(SOURCE_FILES  ${SRC_PATH}/memmove_avx2.c)
else ()
set(SOURCE_FILES ${SRC_PATH}/amd_memmove.c)
endif()

target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})
