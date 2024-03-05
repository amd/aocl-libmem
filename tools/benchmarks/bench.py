#!/usr/bin/python3
"""
 Copyright (C) 2023-24 Advanced Micro Devices, Inc. All rights reserved.

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

sys.path.insert(0, '../tools/benchmarks/internal')
from lbm import LBM
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
        elif(self.MYPARSER['benchmark']=='lbm'):
            LBM_execute = LBM(ARGS=self.ARGS, class_obj=self)
            LBM_execute() #Status:Success/Failure

libmem_funcs = ['memcpy', 'memmove', 'memset', 'memcmp', 'strcpy', 'strncpy', 'strcmp', 'strncmp', 'strlen']

def main():
    """
    Arguments are captured and stored to variable.
    """
    avaliable_cores = subprocess.check_output("lscpu | grep 'CPU(s):' | \
         awk '{print $2}' | head -n 1", shell=True).decode('utf-8').strip()

    parser = argparse.ArgumentParser(prog='bench', description='This\
            program will perform the benchmarking:TBM PBM GBM FBM LBM')
    parser = ParserConfig.add_parser('This\
                            program will perform the benchmarking ')
    parser.add_argument("benchmark", help="select the \
            benchmarking tool for LibMem",
                         type=str,choices = ["tbm", "gbm","fbm","lbm"], default="lbm")
    parser.add_argument("func", help="LibMem function whose performance needs to be analyzed",
                            type=str, choices = libmem_funcs,default="memcpy")

    parser.add_argument("-r", "--range", nargs = 2, help="range of data\
                                lengths to be verified.",
                            type=int, default = [8, 32*1024*1024])
    parser.add_argument("-m", "--mode", help = "type of benchmarking mode:\
                            c - cached, u - un-cached, w - walk, p - page_walk\
                            GBM supports [c,u] & LBM supports [c,u,w,p]",\
                            type = str, choices = ['c', 'u', 'w', 'p'], \
                            default = 'm')
    parser.add_argument("-a", "--align", help = "alignemnt of source\
                                and destination addresses: p - page_aligned,\
                                v - vector_aligned, u - unaligned, c - cache_aligned and d - default alignment\
                                is random.[p,v,u and c are Experimental options]",\
                                type = str, choices = ['p', 'v', 'u', 'c', 'd'],  default = 'd')
    parser.add_argument("-mem_alloc", help = "specify the memory allocator\
                                for FleetBench",type = str,choices=['tcmalloc','glibc'], \
                                default= ['glibc'])

    parser.add_argument("-t", "--iterator", help = "iteration pattern for a \
                            given range of data sizes. Default is shift left\
                            by 1 of starting size - '<<1'.",
                            type = int, default = 0)
    parser.add_argument("-i", "--iterations", help = "Number of iterations for\
                                performance measurement. Default value is \
                                set to 1000 iterations.",
                            type = int, default = 1000)

    """
    parser.add_argument("-g", "--graph", help="Generates the Latency and \
            Throughput Reports by plotting LibMem vs Glibc graphs with gains,\
            Report",type=str,choices=['l'])
    """
    parser.add_argument("-x", "--core_id", help = "CPU core_id on which \
                            benchmark has to be performed. Default choice of \
                            core-id is 8", type = int, default = 8,
                            choices = range(0, int(avaliable_cores)))


    args = ParserConfig.parser_args()

    return args

if __name__ == "__main__":
    try:
    # Check if numactl is installed
        subprocess.check_output(['which', 'numactl'])

    except subprocess.CalledProcessError:
    # numactl is not installed, exiting the program
        print("numactl utility NOT found. Please install it.")
        exit(1)
    myparser = main()
    obj = Bench( ARGS=myparser)
    obj()



