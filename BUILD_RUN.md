# Build & Run Guide for **_AOCL-LibMem_**

## Requirements
 * **cmake 3.11**
 * **python 3.10**
 * **gcc 12.2.0**
 * **aocc 4.0**

## Build Procedure:
### Shared Library:
```sh
    $ mkdir build
    $ cd build
    #Configure for GCC build
        # Default Native Build
        $ cmake -D CMAKE_C_COMPILER=gcc ../aocl-libmem
        # Cross Compiling AVX2 binary on AVX512 machine
        $ cmake -D CMAKE_C_COMPILER=gcc -D ALMEM_ARCH=avx2 ../aocl-libmem
        # Cross Compiling AVX512 binary on AVX2 machine
        $ cmake -D CMAKE_C_COMPILER=gcc -D ALMEM_ARCH=avx512 ../aocl-libmem
        # Enabling Tunable Parameters
        $ cmake -D CMAKE_C_COMPILER=gcc -D ENABLE_TUNABLES=Y ../aocl-libmem
    #Configure for AOCC(Clang) build
        # Default Native Build
        $ cmake -D CMAKE_C_COMPILER=clang ../aocl-libmem
        # Cross Compiling AVX2 binary on AVX512 machine
        $ cmake -D CMAKE_C_COMPILER=clang -D ALMEM_ARCH=avx2 ../aocl-libmem
        # Cross Compiling AVX512 binary on AVX2 machine
        $ cmake -D CMAKE_C_COMPILER=clang -D ALMEM_ARCH=avx512 ../aocl-libmem
        # Enabling Tunable Parameters
        $ cmake -D CMAKE_C_COMPILER=clang -D ENABLE_TUNABLES=Y ../aocl-libmem
    #Build
    $ cmake --build .
    #Install
    $ make install
```

A shared library file 'libaocl-libmem.so' will be generated and stored under 'build/lib/shared/' path.


### Static Library:
```sh
    $ mkdir build
    $ cd build
    #Configure for GCC build
        # Default Native Build
        $ cmake -D CMAKE_C_COMPILER=gcc -D BUILD_SHARED_LIBS=N ../aocl-libmem
        # Cross Compiling AVX2 binary on AVX512 machine
        $ cmake -D CMAKE_C_COMPILER=gcc -D ALMEM_ARCH=avx2 -D BUILD_SHARED_LIBS=N ../aocl-libmem
        # Cross Compiling AVX512 binary on AVX2 machine
        $ cmake -D CMAKE_C_COMPILER=gcc -D ALMEM_ARCH=avx512 -D BUILD_SHARED_LIBS=N ../aocl-libmem
        # Enabling Tunable Parameters
        $ cmake -D CMAKE_C_COMPILER=gcc -D ENABLE_TUNABLES=Y -D BUILD_SHARED_LIBS=N ../aocl-libmem
    #Configure for AOCC(Clang) build
        # Default Native Build
        $ cmake -D CMAKE_C_COMPILER=clang -D BUILD_SHARED_LIBS=N ../aocl-libmem
        # Cross Compiling AVX2 binary on AVX512 machine
        $ cmake -D CMAKE_C_COMPILER=clang -D ALMEM_ARCH=avx2 -D BUILD_SHARED_LIBS=N ../aocl-libmem
        # Cross Compiling AVX512 binary on AVX2 machine
        $ cmake -D CMAKE_C_COMPILER=clang -D ALMEM_ARCH=avx512 -D BUILD_SHARED_LIBS=N ../aocl-libmem
        # Enabling Tunable Parameters
        $ cmake -D CMAKE_C_COMPILER=clang -D ENABLE_TUNABLES=Y -D BUILD_SHARED_LIBS=N ../aocl-libmem
    #Build
    $ cmake --build .
    #Install
    $ make install
```

A static library file 'libaocl-libmem.a' will be generated and stored under 'build/lib/static' path.

## Debug Build:
 To enable logging build the source as below
```sh
    $ cmake -D ENABLE_LOGGING=Y ../aocl-libmem
```
 Logs will be stored in the`"/tmp/libmem.log"` file.

 Enable debugging logs by uncommenting the below line from  "CMakeLists.txt" in root directory.
 _debugging logs_: `add_definitions(-DLOG_LEVEL=4)`

## Running application:
 ``Run the application by preloading the shared 'libaocl-libmem.so' generated from the above build procedure.``
```sh
    $ LD_PRELOAD=<path to build/lib/shared/libaocl-libmem.so> <executable> <params>
```
 * **`WARNING: Do not load/run AVX512 library on Non-AVX512 machine. Running AVX512 on non-AVX512 will lead to crash(invalid instructions).`**

## User Config:
### 1. Default State Run:
 ``Best fit implementation for the underlying ZEN microarchitecture will be chosen by the library.``


### 2. Tunable State Run:

_There are two tunables that will be parsed by libmem._
 * **`LIBMEM_OPERATION`** :- instruction based on alignment and cacheability
 * **`LIBMEM_THRESHOLD`** :- the threshold for ERMS and Non-Temporal instructions

The library will choose the implementation based on the tuned parameter at run time.

#### 2.1. _LIBMEM_OPERATION_ :
**Setting this tunable will let you choose implementation which is a combination of move instructions and alignment of the source and destination addresses.**

 **LIBMEM_OPERATION** format: **`<operations>,<source_alignment>,<destination_alignmnet>`**

 ##### Valid options:
 * `<operations> = [avx2|avx512|erms]`
 * `<source_alignment> = [b|w|d|q|x|y|n]`
 * `<destination_alignmnet> = [b|w|d|q|x|y|n]`

 e.g.:  To use only avx2 based move operations with both unaligned source and destination addresses.
```sh
    LD_PRELOAD=<build/lib/shared/libaocl-libmem.so> LIBMEM_OPERATION=avx2,b,b <executable>
```

#### 2.2. _LIBMEM_THRESHOLD_ :
**Setting this tunable will let us configure the threshold values for the supported instruction set.**

 **LIBMEM_THRESHOLD** format: **`<repmov_start_threshold>,<repmov_stop_threshold>,<nt_start_threshold>,<nt_stop_threshold>`**

 ##### Valid options:
 * `<repmov_start_threshold> = [0, +ve integers]`
 * `<repmov_stop_threshold> = [0, +ve integers, -1]`
 * `<nt_start_threshold> = [0, +ve integers]`
 * `<nt_stop_threshold> = [0, +ve integers, -1]`

 One has to make sure that they provide valid start and stop range values.
 If the size has to be set to maximum length then pass "-1"

 e.g.: To use **REP MOVE** instructions for a range of 1KB to 2KB and non_temporal instructions for a range of 512KB and above.
 ```sh
 LD_PRELOAD=<build/lib/shared/libaocl-libmem.so> LIBMEM_THRESHOLD=1024,2048,524288,-1 <executable>
 ```
 **` Kindly refer to User Guide(docs/User_Guide.md) for the detailed tuning of parameters.`**
