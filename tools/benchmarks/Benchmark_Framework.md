# Build & Run Guide for **_AOCL-LibMem Benchmarking Tool_**

## Description
- AOCL-LibMem Benchmarking Framework is used for performance analysis of LibMem supported functions  against the  Glibc installed host machine under test.
- AOCL-LibMem Benchmarking Tool supports the below benchmark framework and their respecitve modes.
    - LibMemBench : cached, uncached, walk, page & mixed modes

        Internal benchmarking tool which supports different modes for benchmarking.
    - TinyMemBench:

        External benchmark tool which uses the existing tbm framework.
    - GoogleBench: cached & uncached modes

        A microbenchmark support library for running memory benchmarks.
    - FleetBench:

        By making use of existing probability data set, function call to a specific size is being made.

- Result will be generated in the format of csv and .png(in case of graph reports)

## Report Description
  - Performance reports will be stored in this PATH
```
build_dir/test/out/<mem_function>
```
  - Gives the Throughput gains in %age.

## Build_and_Compilers
- Linux:
  - Compilers: AOCC and GCC

## Benchmark Framework Setup
- Python dependency packages
    - numpy  (version 1.24.2 or greater)
    - pandas (verion 1.5.3 or greater)

    STEPS for installing the required packages:

      pip3 install pandas
      pip3 install numpy
- FleetBench dependency package
    - Bazel-version 6 needs to be installed manually

    STEPS for installing Bazel:

        $ sudo apt install curl gnupg
        $ curl -fsSL https://bazel.build/bazel-release.pub.gpg | gpg --dearmor > bazel.gpg
        $ sudo mv bazel.gpg /etc/apt/trusted.gpg.d/
        $ echo "deb [arch=amd64] https://storage.googleapis.com/bazel-apt stable jdk1.8" | sudo tee /etc/apt/sources.list.d/bazel.list
        $ sudo apt update && sudo apt install bazel

## Arguments for the framework

    $ ./bench.py <name_of_the_benchmark> <memory_function> -x<core_id> -r [start] [end] -i<iterations>
        <name_of_the_benchmark> = gbm,tbm,lbm,fbm.
        <memory_function> = memcpy,memset,memmove,memcmp.
        -x <core_id>      =  Enter the CPU core on which you want to run the benchmark.
        -r [start] [end]  = start and end size range in Bytes.(Not applicable for Fleetbench)
        -i <iterations>   = specify the no.of iterations.
