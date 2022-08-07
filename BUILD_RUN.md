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
    #Configure for AOCC(Clang) build
        # Default
        $ cmake -D CMAKE_C_COMPILER=clang ../aocl-libmem
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
