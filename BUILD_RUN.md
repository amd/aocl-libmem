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

`Note:` _Both shared library: 'libaocl-libmem.so' and static library: 'libaocl-libmem.a' are installed under '<build_dir>/lib/' path._

`Note:` _*Dynamic dispatched binary* may not necessarily result into best performance of the library on the given architecture. For the best performance on given architecture use default *native* build option._

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

 **` Kindly refer to`** [User_Guide.md](/docs/User_Guide.md) **`for the detailed tuning of parameters.`**
