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



class GBM:
    def __init__(self, **kwargs):
        self.ARGS = kwargs
        self.MYPARSER = self.ARGS["ARGS"]
        self.func=self.MYPARSER['ARGS']['func']
        self.path="../tools/benchmarks/external/gbench/"
        self.variant=""
        self.isExist=""
        self.memory_operation=self.MYPARSER['ARGS']['mode']
        if self.memory_operation not in ['c','u']:
                self.memory_operation='c'
        self.result_dir =""
        self.variant="amd"
        self.ranges=self.MYPARSER['ARGS']['range']
        self.core=self.MYPARSER['ARGS']['core_id']
        self.bench_name='GooglBench_Cached'

        self.func=self.MYPARSER['ARGS']['func']

    def __call__(self):
        self.isExist=os.path.exists(self.path+"/benchmark")
        if (not self.isExist):
            print("Downloading and Configuring GoogleBench")
            subprocess.run(["git","clone", "https://github.com/google/benchmark.git"],cwd=self.path)
            subprocess.run(["git","clone", "https://github.com/google/googletest.git","benchmark/googletest"],cwd=self.path)
            subprocess.run(['cmake','-E','make_directory','build'],cwd=self.path+"/benchmark")
            subprocess.run(['cmake','-E','chdir','build','cmake','-DBENCHMARK_DOWNLOAD_DEPENDENCIES=on','-DCMAKE_BUILD_TYPE=Release','../'],cwd=self.path+"/benchmark")
            subprocess.run(['cmake','--build','build','--config','Release'],cwd=self.path+"/benchmark")

        self.result_dir = 'out/' + self.func + '/' + \
        datetime.datetime.now().strftime("%d-%m-%Y_%H-%M-%S")
        os.makedirs(self.result_dir, exist_ok=False)
        print(self.result_dir)

        print("Running GoogleBench")
        if self.memory_operation == 'u':
            self.bench_name='GooglBench_UnCached'


        print(self.bench_name)
        subprocess.run(["g++","gbench.cpp","-isystem","benchmark/include","-Lbenchmark/build/src","-lbenchmark","-lpthread","-o","googlebench"],cwd=self.path)
        self.variant="glibc"
        self.gbm_run()
        self.variant="amd"
        self.gbm_run()

        if self.func== 'memcpy':
                self.size_values = subprocess.run(["sed", "-n", r"s/.*cached_memcpy\/\([0-9]\+\).*/\1/p", "gbamd.txt"],cwd =self.result_dir, capture_output=True, text=True).stdout.splitlines()
        elif self.func =='memset':
            self.size_values = subprocess.run(["sed", "-n", r"s/.*cached_memset\/\([0-9]\+\).*/\1/p", "gbamd.txt"],cwd =self.result_dir, capture_output=True, text=True).stdout.splitlines()
        elif self.func =='memmove':
            self.size_values = subprocess.run(["sed", "-n", r"s/.*cached_memmove\/\([0-9]\+\).*/\1/p", "gbamd.txt"],cwd =self.result_dir, capture_output=True, text=True).stdout.splitlines()
        elif self.func =='memcmp':
            self.size_values = subprocess.run(["sed", "-n", r"s/.*cached_memcmp\/\([0-9]\+\).*/\1/p", "gbamd.txt"],cwd =self.result_dir, capture_output=True, text=True).stdout.splitlines()

        self.amd_throughput_values = subprocess.run(["grep", "-Eo", r"[0-9]+(\.[0-9]+)?[MG]/s", "gbamd.txt"],cwd=self.result_dir, capture_output=True, text=True).stdout.splitlines()
        self.glibc_throughput_values = subprocess.run(["grep", "-Eo", r"[0-9]+(\.[0-9]+)?[MG]/s", "gbglibc.txt"],cwd=self.result_dir, capture_output=True, text=True).stdout.splitlines()

        #Converting the G/s values to M/s
        for i in range(len(self.amd_throughput_values)):
            if "G/s" in self.amd_throughput_values[i]:
                self.value = float(self.amd_throughput_values[i].strip("G/s"))
                self.value *= 1024
                self.amd_throughput_values[i] = self.value
            else:
                self.value = float(self.amd_throughput_values[i].strip("M/s"))
                self.amd_throughput_values[i] = self.value

        for i in range(len(self.glibc_throughput_values)):
            if "G/s" in self.glibc_throughput_values[i]:
                self.value = float(self.glibc_throughput_values[i].strip("G/s"))
                self.value *= 1024
                self.glibc_throughput_values[i] = self.value
            else:
                self.value = float(self.glibc_throughput_values[i].strip("M/s"))
                self.glibc_throughput_values[i] = self.value

        print(self.amd_throughput_values)
        print(self.glibc_throughput_values)
        self.gains=[]
        for value in range(len(self.size_values)):
                self.gains.append(round (((self.amd_throughput_values[value] - self.glibc_throughput_values[value] )/ \
                        self.glibc_throughput_values[value] )*100))

        # Open the output file
        with open(self.result_dir+"/"+str(self.bench_name)+"throughput_values.csv", "w",\
                newline="") as output_file:
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
        return True

    def gbm_run(self):
        #print("RUNNING TBM")
        if self.variant =="amd":
            env['LD_PRELOAD'] = '../../../../lib/shared/libaocl-libmem.so'
            print("LIBMEM:",self.func)
        else:
            env['LD_PRELOAD'] = ''
            print("GLIBC:",self.func)

        with open(self.result_dir+'/gb'+str(self.variant)+'.txt','w') as g:
            subprocess.run(["taskset", "-c", str(self.core),"numactl","-C"+str(self.core),"./googlebench","--benchmark_counters_tabular=true",str(self.func),str(self.memory_operation),str(self.ranges[0]),str(self.ranges[1])],cwd=self.path,env=env,check=True,stdout =g)
        return

