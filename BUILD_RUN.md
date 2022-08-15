# Build & Run Guide for **_AOCL-LibMem_**

## Requirements
 * **cmake 3.10**
 * **python 3.6**
 * **gcc 11.1**
 * **aocc 3.2**

## Build Procedure:
```sh
    $ mkdir build
    $ cd build
    #Configure for GCC build
        # Default
        $ cmake ../aocl-libmem
        # Enabling Tunable Parameters
        $ cmake -D ENABLE_TUNABLES=Y ../aocl-libmem
    #Configure for AOCC(Clang) build
        # Default
        $ cmake -D CMAKE_C_COMPILER=clang ../aocl-libmem
        # Enabling Tunable Parameters
        $ cmake -D CMAKE_C_COMPILER=clang -D ENABLE_TUNABLES=Y ./aocl-libmem
    #Build
    $ cmake --build .
    #Install
    $ make install
```

A shared library file libaocl-libmem.so will be generated and stored under build/lib path.

## Debug Build:
 To enable logging build the source as below
```sh
    $ cmake -D ENABLE_LOGGING=Y ../aocl-libmem
```
 Logs will be stored in the`"/tmp/libmem.log"` file.

 Enable debugging logs by uncommenting the below line from  "CMakeLists.txt" in root directory.
 _debugging logs_: `add_definitions(-DLOG_LEVEL=4)`

## Running application:
 ``Run the application by preloading the libaocl-libmem.so generated from the above build procedure.``
```sh
    $ LD_PRELOAD=<path to build/lib/libaocl-libmem.so> <executable> <params>
```

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
 * `<operations> = [avx2|erms]`
 * `<source_alignment> = [b|w|d|q|x|y|n]`
 * `<destination_alignmnet> = [b|w|d|q|x|y|n]`

 e.g.:  To use only avx2 based move operations with both unaligned source and destination addresses.
```sh
    LD_PRELOAD=<build/lib/libaocl-libmem.so> LIBMEM_OPERATION=avx2,b,b <executable>
```
