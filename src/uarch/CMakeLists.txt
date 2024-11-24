# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. Neither the name of the copyright holder nor the names of its contributors
#    may be used to endorse or promote products derived from this software without
#    specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
# OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

set(UARCH_SUPP_FUNCS ${DYN_DISP_SUPP_FUNCS})

add_library(uarch_objlib INTERFACE)

if (ALMEM_DYN_DISPATCH)
    set(uarch_list RANGE ${MIN_ZEN_VER} ${MAX_ZEN_VER})
else ()
    set(uarch_list ${ARCH_VER})
endif()

foreach(uarch ${uarch_list})

    set(UARCH_SRC_FILES "")
    foreach(func ${UARCH_SUPP_FUNCS})
    list(APPEND UARCH_SRC_FILES
        zen${uarch}/${func}_zen${uarch}.c
        )
    endforeach()

    add_library(lib_zen${uarch} OBJECT ${UARCH_SRC_FILES})
    target_include_directories(lib_zen${uarch} PUBLIC ${CMAKE_SOURCE_DIR}/include)

    set(_MODULE_CC_FLAGS "")
    if (${CONFIG_COMPILER_HAS_ZEN${uarch}})
        list(APPEND _MODULE_CC_FLAGS -march=znver${uarch})
    else ()
        message(WARNING " *** Compiler doesn't support arch specific flag for zen${uarch} CPU\n"
                        "Proceeding with generic ISA flags... ")
        if (ALMEM_DYN_DISPATCH)
            if (${uarch} GREATER_EQUAL 4)
                list(APPEND _MODULE_CC_FLAGS -mavx512f -mavx512bw)
            else ()
                list(APPEND _MODULE_CC_FLAGS -mavx2)
            endif ()
        else ()
            if (${HAS_AVX512})
                list(APPEND _MODULE_CC_FLAGS -mavx512f -mavx512bw)
            else ()
                list(APPEND _MODULE_CC_FLAGS -mavx2)
            endif ()
        endif()
        list(APPEND _MODULE_CC_FLAGS -mbmi -mlzcnt)
    endif ()

    set_target_properties(lib_zen${uarch} PROPERTIES POSITION_INDEPENDENT_CODE "ON" COMPILE_OPTIONS "${_MODULE_CC_FLAGS}")
    target_sources(uarch_objlib INTERFACE $<TARGET_OBJECTS:lib_zen${uarch}>)
endforeach()
