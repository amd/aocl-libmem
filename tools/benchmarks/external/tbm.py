"""
 Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

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



class TBM:
    def __init__(self, **kwargs):
        self.ARGS = kwargs
        self.MYPARSER = self.ARGS["ARGS"]
        self.func=self.MYPARSER['ARGS']['func']
        self.path="../tools/benchmarks/external/tinybench"
        self.variant=""
        self.isExist=""
        self.result_dir =""
        self.variant="amd"
        self.ranges=self.MYPARSER['ARGS']['range']
        self.core=self.MYPARSER['ARGS']['core_id']
        self.bench_name='TinyMemBench'
        self.func=self.MYPARSER['ARGS']['func']

    def __call__(self):
        self.isExist=os.path.exists(self.path+'/tinymembench')
        if (not self.isExist):
            print("Preparing Tinymem benchmark")
            subprocess.run(["git","clone", "https://github.com/ssvb/tinymembench.git"],cwd=self.path)
            subprocess.run(["make"],cwd=self.path+"/tinymembench")
            os.system("cp ../tools/benchmarks/external/tinybench/tinymem-bench.c ../tools/benchmarks/external/tinybench/tinymembench/")
            subprocess.run(["gcc","-O2","-o","tinymembench","tinymem-bench.c",\
                    "util.o","asm-opt.o","x86-sse2.o","-lm"],cwd=self.path+"/tinymembench")
            print("prepared TINY BENCH")

        self.result_dir = 'out/' + self.func + '/' + \
        datetime.datetime.now().strftime("%d-%m-%Y_%H-%M-%S")
        os.makedirs(self.result_dir, exist_ok=False)
        print("RUNNING TBM")
        self.variant="glibc"
        self.tiny_run()
        self.variant="amd"
        self.tiny_run()

        with open(self.result_dir+"/amd.txt", "r") as input_file:
            self.data = input_file.read()
        self.size_values = subprocess.run(["sed", "-n", r"s/SIZE: \([0-9]*\) B.*/\1/p", "amd.txt"],cwd =self.result_dir, capture_output=True, text=True).stdout.splitlines()

        # Extract the throughput values using the sed command
        self.amd_throughput_values = subprocess.run(["sed", "-n", r"s/.*:\s*\([0-9.]*\) MB\/s.*/\1/p", "amd.txt"],cwd=self.result_dir, capture_output=True, text=True).stdout.splitlines()
        self.glibc_throughput_values = subprocess.run(["sed", "-n", r"s/.*:\s*\([0-9.]*\) MB\/s.*/\1/p", "glibc.txt"],cwd=self.result_dir, capture_output=True, text=True).stdout.splitlines()
        self.gains = []

        self.amd = [eval(i) for i in self.amd_throughput_values]
        self.glibc = [eval(i) for i in self.glibc_throughput_values]

        for value in range(len(self.size_values)):
            self.gains.append(round (((self.amd[value] - self.glibc[value] )/ self.glibc[value] )*100))

        # Open the output file
        with open(self.result_dir+'/'+str(self.bench_name)+"throughput_values.csv",\
                "w", newline="") as output_file:
            writer = csv.writer(output_file)
            # Write the values to the CSV file
            writer.writerow(["Size", "amd_Throughput","glibc_Throughput",\
                    "GAINS"])
            for size, athroughput , gthroughput, g  in zip(self.size_values, \
                    self.amd_throughput_values,self.glibc_throughput_values,self.gains):
                    writer.writerow([size, athroughput,gthroughput,g])

        self.print_result()
        return


    def print_result(self):
        input_file = read_csv(self.result_dir+"/"+str(self.bench_name)+\
            "throughput_values.csv")
        self.size = input_file['Size'].values.tolist()
        self.gains = input_file['GAINS'].values.tolist()
        print("\nBENCHMARK: "+self.bench_name)
        print("    SIZE".ljust(8)+"     : GAINS")
        print("    ----------------")
        for x in range(len(self.size)):
            if int(self.size[x]) >= 1024*1024:
                print("   ",((str(int(self.size[x])/(1024*1024)))+" MB").ljust(8)+" :"+\
                    (str(self.gains[x])+"%").rjust(6))
            elif int(self.size[x]) >= 1024:
                print("   ",((str(int(self.size[x])/(1024)))+" KB").ljust(8)+" :"+(str\
                    (self.gains[x])+"%").rjust(6))
            else:
                print("   ",(str(int(self.size[x])) + " B").ljust(8)+" :"+(str\
                    (self.gains[x])+"%").rjust(6))

        print("\n*** Test reports copied to directory ["+self.result_dir+"] ***\n")
        return

    def tiny_run(self):
        if self.variant =="amd":
            env['LD_PRELOAD'] = '../../../../../lib/shared/libaocl-libmem.so'
            print("LIBMEM:",self.func)
        else:
            env['LD_PRELOAD'] = ''
            print("GLIBC:",self.func)

        with open(self.result_dir+'/'+str(self.variant)+'.txt', 'w') as f:
            subprocess.run(['taskset', '-c',str(self.core),'numactl','-C'+str(self.core),'./tinymembench',str(self.func),\
                str(self.ranges[0]),str(self.ranges[1])],cwd=self.path+"/tinymembench",\
                env=env, stdout=f)

        return

