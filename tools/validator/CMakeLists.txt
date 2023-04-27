file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Validator_tool.md
DESTINATION ${CMAKE_BINARY_DIR}/tools/validator/)

install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/validator.py
  DESTINATION ${CMAKE_BINARY_DIR}/test)

add_executable(libmem_validator ${CMAKE_CURRENT_SOURCE_DIR}/libmem_validator.c)
