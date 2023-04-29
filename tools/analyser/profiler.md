# Guide for **_AOCL-LibMem Profiler Tool_**
## Description
- AOCL-LibMem Profiler tool  is used for profiling a target application , which gives insightful information by trapinh all calls to libc (memcpy, mempcpy, memcmp, memmove, memset) function on a system, and gathers a histogram of the size arguments to these calls, reporting back at some interval.

## LibMem Profiler

## Requirements:
- python bcctools [python3-bpfcc]

        $sudo apt-get install -y python3-bpfcc

## Running the Profiler Script

        $sudo ./prof_libmem.py -v -p <pid-of-running-application>


