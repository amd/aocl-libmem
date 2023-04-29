# Guide for **_AOCL-LibMem Validator Tool_**

## Description
- AOCL-LibMem Validator Framework is used for functional validation of LibMem supported functions.
- It performs data validation check , which checks the data integrity of the source and destination memory.
- By default the tool checks for validation of standard sizes with source and destination alignment as cache line SIZE . However , for non - standard sizes the user can pass the size range  along with the -t <iterator> value for validating the target sizes with different combinations of source and destination alignment.
- Result will be generated in the format of csv file.

## Report Description
  - Validation report will be stored in this PATH
```
build_dir/test/out/<libmem_function>/<time-stamp-counter>/<validation_report.csv>
```
  - For the given Input SIZE the tool shows the Validation result as
Passed / Failed.

## Arguments for the framework

    $ ./validator.py -r [start] [end] -a[src] [dst] -t[iterator count] <mem_function>
        -r Range       = [Start] and [End] range in Bytes.
        -a alignment   = [src] and [dst] alignments. Default alignment is 64B for both source and destination.
        -t <iterator>  = specify the iteration pattern. Default is "2x" of starting size - '<<1'
        <mem_function> = memcpy,memset,memcmp,memmove,mempcpy,strcpy.

## Example
        Sample Run: $./validator.py -r 8 64 -a 5 8 -t"+1" memcpy
        Perfroms the Validation testing starting from 8 Bytes --> 64 Bytes
        with Src Alignment: 5 & Dst Alignment: 8
        and iterator pattern as "+1".

        > Validation for size [8] in progress...
        > Validation for size [9] in progress...
        > Validation for size [10] in progress...
        ...



The tester can provide different options as needed. Please refer to the help message
of the python script to set up  the validator config.
```sh
   $ ./validator.py -h
```
