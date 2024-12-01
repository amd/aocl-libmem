# AOCL-LibMem
AOCL-LibMem is a Linux library of data movement and manipulation functions (such as memcpy
and strcpy) highly optimized for AMD Zen micro-architecture. This library has multiple
implementations of each function that can be chosen based on the application requirements as per
alignments, instruction choice, threshold values, and tunable parameters. By default, this library will
choose the best fit implementation based on the underlying micro-architectural support for CPU
features and instructions.

This release of AOCL-LibMem library supports the following functions:
+ memcpy
+ mempcpy
+ memmove
+ memset
+ memcmp
+ memchr
+ strcpy
+ strncpy
+ strcmp
+ strncmp
+ strlen
+ strcat
+ strstr
+ strchr

# INSTALLATION
INSTALLATION FROM AOCL-LIBMEM GIT REPOSITORY:

After downloading the latest stable release from the git repository, https://github.com/amd/aocl-libmem, follow the steps from the BUILD_RUN.md, to configure and build it for  all released AMD Zen CPU architecutres.

`Note:` _Binaries can be built based on the cpu ISA features: AVX2 and AVX512 with help of Cmake flag: ALMEM_ARCH. It is recommended not to load/run the AVX512 library on a non-AVX512 machine as it will lead to crash due to unsupported instructions._

For detailed instructions on how to configure, build, install, please refer to the AOCL User Guide on https://developer.amd.com/amd-aocl

Upstream repository contains LibMem reference manual that has all the information you need to get started with LibMem, including installation directions, usage examples, and a complete API reference.You may also find a copy of the document here on github.

## Build and Run:
**Refer to [BUILD_RUN](https://github.com/amd/aocl-libmem/blob/main/BUILD_RUN.md)**

## User Guide:
**Refer to [User_Guide](https://github.com/amd/aocl-libmem/blob/main/docs/User_Guide.md)**

## Validator Tool Guide:
**Refer to tools/validator/Validator_tool.md**

## AOCL-LibMem Benchmarking Tool Guide:
**Refer to tools/benchmarks/Benchmark_Framework.md**

Also, please read the LICENSE file for information on copying and distributing this software.

# Technical Support
Please email toolchainsupport@amd.com for questions, issues and feedback on LibMem.

Please submit your questions, feature requests, and bug reports on the [GitHub issues](https://github.com/amd/aocl-libmem/issues) page.
