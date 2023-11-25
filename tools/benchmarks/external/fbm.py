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
from libmem_defs import *


class FBM:
    def __init__(self, **kwargs):
        self.ARGS = kwargs
        self.MYPARSER = self.ARGS["ARGS"]
        self.func=self.MYPARSER['ARGS']['func']
        self.path="../tools/benchmarks/external/"
        self.variant=""
        self.isExist=""
        self.result_dir =""
        self.variant="amd"
        self.ranges=self.MYPARSER['ARGS']['range']
        self.core=self.MYPARSER['ARGS']['core_id']
        self.bench_name='FleeteBench'
        self.last=9
        self.allocator=self.MYPARSER['ARGS']['mem_alloc']
        self.amd_throughput_value=[]
        self.glibc_throughput_value=[]
        self.amd_raw=[]
        self.glibc_raw=[]
        self.gains=[]
        self.data=["Google A","Google B","Google D",
                   "Google L","Google M","Google Q",
                   "Google S","Google U","Google W"]

        self.func=self.MYPARSER['ARGS']['func']
        self.LibMemVersion=''
        self.GlibcVersion=''


    def __call__(self):
        self.isExist=os.path.exists(self.path+'/fleetbench')
        if (not self.isExist):
            print("Preparing Fleetbench benchmark")
            subprocess.run(["git","clone","-c","advice.detachedHead=false","-b","v0.2","https://github.com/google/fleetbench.git"],cwd=self.path)
            subprocess.run(["bazel","build","-c","opt","//fleetbench/libc:mem_benchmark","--cxxopt=-Wno-deprecated-declarations","--cxxopt=-Wno-unused-variable"],cwd=self.path+"/fleetbench")

        self.result_dir = 'out/'+self.bench_name+'/'+self.func + '/' + \
        datetime.datetime.now().strftime("%d-%m-%Y_%H-%M-%S")
        os.makedirs(self.result_dir, exist_ok=False)
        print("Benchmarking of "+str(self.func)+" for DATA Distributions["+str(self.data[0])+"-"+str(self.data[-1])+"] on "+str(self.bench_name))

        self.variant="glibc"
        self.fbm_run()
        self.variant="amd"
        self.fbm_run()

        for i in range(0,self.last):
            self.glibc_raw.append(subprocess.run(["sed", "-n", r"/mean/{s/.*bytes_per_second=\([^ ]*\).*/\1/p;q}", "fb_"+str(i)+"glibc.txt"],cwd=self.result_dir, capture_output=True, text=True).stdout.splitlines())
            self.amd_raw.append(subprocess.run(["sed", "-n", r"/mean/{s/.*bytes_per_second=\([^ ]*\).*/\1/p;q}", "fb_"+str(i)+"amd.txt"],cwd=self.result_dir, capture_output=True, text=True).stdout.splitlines())

        self.amd_raw=[item[0] for item in self.amd_raw]
        self.glibc_raw=[item[0] for item in self.glibc_raw]

        #Converting the M/s values to G/s
        self.amd_throughput_value=self.throughput_converter(self.amd_raw)
        self.glibc_throughput_value=self.throughput_converter(self.glibc_raw)

        self.gains=[]
        for value in range(len(self.amd_throughput_value)):
                self.gains.append(str(round (((self.amd_throughput_value[value] - self.glibc_throughput_value[value] )/ \
                        self.glibc_throughput_value[value] )*100))+str('%'))

        # Open the output file
        with open(self.result_dir+"/"+str(self.bench_name)+"throughput_values.csv", "w",\
                newline="") as output_file:
            writer = csv.writer(output_file)
            # Write the values to the CSV file
            writer.writerow(["Size","Glibc-"+str(self.GlibcVersion,'utf-8').strip(),"LibMem-"+str(self.LibMemVersion,'utf-8').strip(),\
                    "GAINS"])
            for size, gthroughput , athroughput, g  in zip(self.data, \
                    self.glibc_throughput_value,self.amd_throughput_value,self.gains):
                    writer.writerow([size, gthroughput,athroughput,g])

        self.print_result()
        return True

    def throughput_converter(self,value):
        print(type(value))
        print("......",value)
        for i in range(len(value)):
            if 'G/s' in value[i]:
                self.value = value[i].strip("G/s")
                value[i]=float(self.value)
            else:
                self.value = float(value[i].strip("M/s"))
                self.value /= 1000
                value[i]=float(self.value)
        return value

    def print_result(self):
        input_file = read_csv(self.result_dir+"/"+str(self.bench_name)+\
            "throughput_values.csv")
        self.size = input_file['Size'].values.tolist()
        self.gains = input_file['GAINS'].values.tolist()
        print("\nBENCHMARK: "+self.bench_name)
        print("    SIZE".ljust(8)+"     : GAINS")
        print("    ----------------")
        for x in range(len(self.size)):
                print("   ",(str(self.size[x])).ljust(8)+" :"+\
                    (str(self.gains[x])).rjust(6))

        print("\n*** Test reports copied to directory ["+self.result_dir+"] ***\n")
        return

    def fbm_run(self):
        if self.variant =="amd":
            self.LibMemVersion = subprocess.check_output("file " + LIBMEM_BIN_PATH + \
                "| awk -F 'so.' '/libaocl-libmem.so/{print $3}'", shell =True)
            #Setting up the Absolute path for LD_PRELOAD
            mycwd=os.getcwd()
            env['LD_PRELOAD'] = LIBMEM_BIN_PATH

            print("FBM : Running Benchmark on AOCL-LibMem "+str(self.LibMemVersion,'utf-8').strip())
        else:
            self.GlibcVersion = subprocess.check_output("ldd --version | awk '/ldd/{print $NF}'", shell=True)
            env['LD_PRELOAD'] = ''
            print("FBM : Running Benchmark on GLIBC "+str(self.GlibcVersion,'utf-8').strip())

        #Adding Tcmalloc Allocator
        if(self.allocator == 'tcmalloc'):
            env['GLIBC_TUNABLES']='glibc.pthread.rseq=0'

        for i in range(0,self.last):
            with open(self.result_dir+'/fb_'+str(i)+str(self.variant)+'.txt','w') as g:
                subprocess.run(["taskset","-c", str(self.core),"numactl","-C"+str(self.core),"bazel","run","--config=opt","fleetbench/libc/mem_benchmark","--","--benchmark_filter=BM_Memory/"+str(self.func)+"/"+str(i),"--benchmark_repetitions=300"], cwd=self.path+"/fleetbench",env=env,check=True,stdout =g,stderr=subprocess.PIPE)
        return

