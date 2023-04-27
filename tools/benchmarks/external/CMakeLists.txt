# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
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


#copy source files to build/test directory
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/tbm.py
 DESTINATION ${CMAKE_BINARY_DIR}/tools/benchmarks/external/)
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/fbm.py
 DESTINATION ${CMAKE_BINARY_DIR}/tools/benchmarks/external/)
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/gbm.py
 DESTINATION ${CMAKE_BINARY_DIR}/tools/benchmarks/external/)
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/GetParser.py
 DESTINATION ${CMAKE_BINARY_DIR}/tools/benchmarks/external/)

# Below source files will be built along the fetched Google bench Source
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/gbench/gbench.cpp
 DESTINATION ${CMAKE_BINARY_DIR}/tools/benchmarks/external/gbench)

# Below source files will be built along the fetched Tinymem bench Source
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/tinybench/tinymem-bench.c
 DESTINATION ${CMAKE_BINARY_DIR}/tools/benchmarks/external/tinybench)

