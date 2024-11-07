# Build & Run Guide for **_AOCL-LibMem_**

## Requirements
 * **cmake 3.22**
 * **python 3.10**
 * **gcc 12.2.0**
 * **aocc 4.0**

## Build and Install Procedure:
```sh
    #Configure for GCC build
        # Default Option - Compiling for Native CPU
        $ cmake -D CMAKE_C_COMPILER=gcc -S <source_dir> -B <build_dir>
        # Compiling with Dynamic CPU Dispatcher
        $ cmake -D CMAKE_C_COMPILER=gcc -D ALMEM_DYN_DISPATCH=Y -S <source_dir> -B <build_dir>
        # Compiling AVX2 binary
        $ cmake -D CMAKE_C_COMPILER=gcc -D ALMEM_ISA=avx2 -S <source_dir> -B <build_dir>
        # Compiling AVX512 binary
        $ cmake -D CMAKE_C_COMPILER=gcc -D ALMEM_ISA=avx512 -S <source_dir> -B <build_dir>
        # Compiling with Tunable Parameters
        $ cmake -D CMAKE_C_COMPILER=gcc -D ALMEM_TUNABLES=Y -S <source_dir> -B <build_dir>
    #Configure for AOCC(Clang) build
        # Default Option - Compiling for Native CPU
        $ cmake -D CMAKE_C_COMPILER=clang -S <source_dir> -B <build_dir>
        # Compiling with Dynamic CPU Dispatcher
        $ cmake -D CMAKE_C_COMPILER=clang -D ALMEM_DYN_DISPATCH=Y -S <source_dir> -B <build_dir>
        # Compiling AVX2 binary
        $ cmake -D CMAKE_C_COMPILER=clang -D ALMEM_ISA=avx2 -S <source_dir> -B <build_dir>
        # Compiling AVX512 binary
        $ cmake -D CMAKE_C_COMPILER=clang -D ALMEM_ISA=avx512 -S <source_dir> -B <build_dir>
        # Compiling with Tunable Parameters
        $ cmake -D CMAKE_C_COMPILER=clang -D ALMEM_TUNABLES=Y -S <source_dir> -B <build_dir>
    Note: 'ALMEM_ISA' has precedence over 'ALMEM_DYN_DISPATCH' and 'ALMEM_TUNABLES' options.
    # Build
    $ cmake --build <build_dir>

    # Install
    $ cmake --install <build_dir>
    ## For custom install path, run configure with "CMAKE_INSTALL_PREFIX"
```

Both shared library: 'libaocl-libmem.so' and static library: 'libaocl-libmem.a' are installed under '<build_dir>/lib/' path.


## Debug Build:
 To enable logging build the source as below
```sh
    $ cmake -D ALMEM_LOGGING=Y -S <source_dir> -B <build_dir>
```
 Logs will be stored in the`"/tmp/libmem.log"` file.

 Enable debugging logs by uncommenting the below line from  "CMakeLists.txt" in root directory.
 _debugging logs_: `add_definitions(-DLOG_LEVEL=4)`

## Running application:
 ``Run the application by preloading the shared 'libaocl-libmem.so' generated from the above build procedure.``
```sh
    $ LD_PRELOAD=<path to build/lib/libaocl-libmem.so> <executable> <params>
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
    LD_PRELOAD=<build/lib/libaocl-libmem.so> LIBMEM_OPERATION=avx2,b,b <executable>
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
 LD_PRELOAD=<build/lib/libaocl-libmem.so> LIBMEM_THRESHOLD=1024,2048,524288,-1 <executable>
 ```
 **` Kindly refer to User Guide(docs/User_Guide.md) for the detailed tuning of parameters.`**
