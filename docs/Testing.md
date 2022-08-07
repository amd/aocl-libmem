# Testing Guide for *AOCL-LibMem*


## Testing AOCL-LibMem functions

A generic test framework is implemented in python which does the following
tasks for a specified string function over a range of data lengths along
with alignments of source and destination addresses.
1) data validation: check the data integrity of source and destination memory.
2) latency measurement: measure time to perform string operations in 'nsec'.
3) performance analysis: performance of libmem function against Glibc.

AOCL-LibMem test script will be available in the build directories test folder.

Run the below command under the test directory to know the usage of the test script
```sh
   $ cd test
   $ python3 test_libmem.py -h
```

Results of the test operations will be stored in a path of the format
    *`test/out/<str_func>/<date-time>/`*
The following list of files will be available in the out folder:
1) test config
2) latency reports
3) performance report
4) validation report
5) test summary


## Running the test framework

Run the below command to test memcpy with libmem test framework.
```sh
   $ cd test
   $ python3 test_libmem.py memcpy
```
The above command will perform data validation, latency measurement and
generate a performance comparison report against the Glibc version
installed on the system for memcpy.

Replace `memcpy` with the string function of your interest from the supported list.

The default configuration of the test run:
args|values
----|-----
string_function |memcpy
data_range[start, end]| (8B, 33554432B)
alignment(src, dst) |(32b, 32b)
iterator | <<1
iterations | 1000
This test configuration of every run will be available under the out folder as
mentioned above.

The tester can provide different options as needed. Please refer to the help message
of the python script to set the test config.
```sh
   $ python3 test_libmem.py -h
```

`Note: ` Set the machine to FF mode for accurate performance results.
