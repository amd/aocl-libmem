#!/usr/bin/python3
"""
 Copyright (C) 2023-25 Advanced Micro Devices, Inc. All rights reserved.

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:
 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
 3. Neither the name of the copyright holder nor the names of its contributors
    may be used to endorse or promote products derived from this software without
    specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
"""


import subprocess
import os
from os import environ as env
from pandas import read_csv
import argparse
import re
import csv
import datetime
from statistics import mean
import sys

sys.path.insert(0, '../tools/benchmarks/external')
import GetParser as ParserConfig
from gbm import GBM
from fbm import FBM
from tbm import TBM


class Bench:
    """
    The object of this class will run the specified benchmark framework
    based on the command given by the user
    """

    def __init__(self, **kwargs):
        self.ARGS = kwargs
        self.MYPARSER = self.ARGS["ARGS"]
        #print(self.MYPARSER)

    def __call__(self, *args, **kwargs):
        if(self.MYPARSER['benchmark']=='tbm'):
            TBM_execute = TBM(ARGS=self.ARGS, class_obj=self)
            TBM_execute() #Status:Success/Failure
        elif(self.MYPARSER['benchmark']=='gbm'):
            GBM_execute = GBM(ARGS=self.ARGS, class_obj=self)
            GBM_execute() #Status:Success/Failure
        elif(self.MYPARSER['benchmark']=='fbm'):
            FBM_execute = FBM(ARGS=self.ARGS, class_obj=self)
            FBM_execute() #Status:Success/Failure
libmem_memory = ['memcpy', 'memmove', 'memset', 'memcmp']
libmem_string = ['strcpy', 'strncpy', 'strcmp', 'strncmp', 'strlen', 'strcat', 'strncat', 'strspn', 'strstr', 'memchr', 'strchr']
libmem_funcs = libmem_memory + libmem_string

def main():
    """
    Arguments are captured and stored to variable.
    """
    available_cores = subprocess.check_output("lscpu | grep 'CPU(s):' | \
         awk '{print $2}' | head -n 1", shell=True).decode('utf-8').strip()

    parser = argparse.ArgumentParser(prog='bench', description='This program will perform the benchmarking: TBM, GBM, FBM')

    # Create subparsers for different benchmarking tools
    subparsers = parser.add_subparsers(dest='benchmark', required=True)

    # Common arguments for all benchmarks
    common_parser = argparse.ArgumentParser(add_help=False)

    common_parser.add_argument("func", help="LibMem supported functions",
                            type=str, choices = libmem_funcs,default="memcpy")

    common_parser.add_argument("-r", "--range", nargs = 2, help="range of data\
                                lengths to be benchmarked.\
                                Memory functions [8 - 32MB]\
                                String functions [8 - 4KB]",
                            type=int)

    common_parser.add_argument("-t", "--iterator", help = "iteration pattern for a \
                            given range of data sizes. Default is shift left\
                            by 1 of starting size - '<<1'.",
                            type = int, default = 0)

    common_parser.add_argument("-x", "--core_id",
                               help=f"CPU core_id on which benchmark has to be performed.\
                            Default choice of core-id is 8. Valid range is \
                            [0..{int(available_cores) - 1}]",
                            type=int,default=8)

    # Subparser for GBM with additional options
    gbm_parser = subparsers.add_parser('gbm', parents=[common_parser], help='GBM Benchmarking Tool')
    group = gbm_parser.add_mutually_exclusive_group()
    gbm_parser.add_argument("-m", "--mode", help = "type of benchmarking mode:\
                            c - cached, u - un-cached",\
                            type = str, choices = ['c', 'u'], \
                            default = 'c')

    #Align and Page are mutually_exclusive options
    group.add_argument("-a", "--align", help = "alignemnt of source\
                                and destination addresses: a - aligned\
                                u - unaligned, and d - default alignment\
                                is random.",\
                                type = str, choices = ['a','u','d'],  default = 'd')

    group.add_argument("-p", "--page", help = "Page boundary options\
                                x - page_cross, t - page_tail",\
                                type = str, choices = ['x','t'],  default = 'n')

    gbm_parser.add_argument("-s", "--spill", help = "cache spilling mode applicable only for\
                                aligned and unaligned: l - less_spill,\
                                m - more_spill, default is no-spill",\
                                type = str, choices = ['l', 'm',],  default = 'n')

    gbm_parser.add_argument("-i", "--repetitions", help = "Number of repitations for\
                            performance measurement. Default value is \
                            set to 10 iterations.",
                        type = int, default = 10)

    gbm_parser.add_argument("-w", "--warm_up", help = "time in seconds\
                                Default value is set to 1sec.",
                            type = float, default = 1)

    gbm_parser.add_argument("-preload", help = "Enables LD_PRELOAD for running bench",
                          type = str, choices = ['y', 'n'], default = 'y')

    gbm_parser.add_argument("-perf", help = "performance runs for LibMem.\
                            Default is benchmarking mode against system Libc",
                            type = str, choices = ['p', 'b'], default = 'b')

    # Subparser for TBM
    tbm_parser = subparsers.add_parser('tbm', parents=[common_parser], help='TBM Benchmarking Tool')

    # Subparser for FBM
    fbm_parser = subparsers.add_parser('fbm', parents=[common_parser], help='FBM Benchmarking Tool')

    fbm_parser.add_argument("-mem_alloc", help="specify the memory allocator for FleetBench",
                               type=str, choices=['tcmalloc', 'glibc'], default='glibc')

    fbm_parser.add_argument("-i", "--repetitions", help = "Number of repitations for\
                            performance measurement. Default value is \
                            set to 100 iterations.",
                        type = int, default = 100)

    args = parser.parse_args()

    # Set the default range based on the func argument
    if args.range is None:
        if args.func in libmem_string:
            args.range = [8, 4096]
        else:
            args.range = [8, 32 * 1024 * 1024]

    return vars(args)

if __name__ == "__main__":
    try:
        # Check if numactl is installed
        subprocess.check_output(['which', 'numactl'])
    except subprocess.CalledProcessError:
        # numactl is not installed, exiting the program
        print("numactl utility NOT found. Please install it.")
        exit(1)

    myparser = main()

    obj = Bench(ARGS=myparser)
    obj()
