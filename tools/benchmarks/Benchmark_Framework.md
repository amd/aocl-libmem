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
build_dir/test/out/<libmem_function>/<time-stamp-counter>/
```
  - Gives the Throughput gains in %age.

## Build_and_Compilers
- Linux:
  - Compilers: AOCC and GCC

## Benchmark Framework Setup
- Python 3.10 and above
- Python dependency packages
    - numpy  (version 1.24.2 or greater)
    - pandas (verion 1.5.3 or greater)

    STEPS for installing the required packages:

      pip3 install pandas
      pip3 install numpy
- FleetBench dependency package
    - Bazel-version 6 needs to be installed manually

    STEPS for installing Bazel:

    On Ubuntu :

        $ sudo apt install curl gnupg
        $ curl -fsSL https://bazel.build/bazel-release.pub.gpg | gpg --dearmor > bazel.gpg
        $ sudo mv bazel.gpg /etc/apt/trusted.gpg.d/
        $ echo "deb [arch=amd64] https://storage.googleapis.com/bazel-apt stable jdk1.8" | sudo tee /etc/apt/sources.list.d/bazel.list
        $ sudo apt update && sudo apt install bazel

    On CentOS and REHL :

  1.    Install dependency packages:

       $ sudo yum install epel-release
       $ sudo yum install gcc gcc-c++ java-1.8.0-openjdk-devel
  2.    Download the Bazel 6 binary installer from the Bazel releases page:

	   $ wget https://github.com/bazelbuild/bazel/releases/download/6.0.0/bazel-6.0.0-installer-linux-x86_64.sh
  3.    Install commands

       $ chmod +x bazel-6.0.0-installer-linux-x86_64.sh
       $ sudo ./bazel-6.0.0-installer-linux-x86_64.sh


## Running Bench framework

    $ ./bench.py <benchmark_name> <memory_function> -m <mode> -x<core_id> -r [start] [end] -t "<iterator_value>" -i<iterations>
        <benchmark_name>  = gbm,tbm,lbm,fbm.
        <memory_function> = memcpy,memset,memmove,memcmp,strcpy,strncpy.
        -m <Mode>         = c,u,p,w (LBM) and c,u(GBM)
        -x <core_id>      =  Enter the CPU core on which you want to run the benchmark.
        -r [start] [end]  = start and end size range in Bytes.(Not applicable for Fleetbench)
        -t "iter_value"   = increments the start size by "value".(0 stands for size<<1; other +ve integers stands for incremental iterations.)
        -i <iterations>   = specify the no.of iterations.

    Example:
    Running LibMem bench framework
    $ ./bench.py lbm memcpy -r 8 4096  -i 1000 -x 16
    Runs the LBM benchmark for size[8,16,32,64...] with 1000 iterations on core -16
    Here default iter_value 0 does size<<1
    $ ./bench.py gbm memcpy -r 8 16 -m c -t "1" -x 16
    Runs the Google Benchmark for Cached Mode Memcpy for sizes[8,9,..16] on core -16


    Benchmark Help option
    $ ./bench.py -h
