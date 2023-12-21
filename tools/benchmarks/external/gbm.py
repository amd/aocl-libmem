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
        self.iterator = self.MYPARSER['ARGS']['iterator']
        self.func=self.MYPARSER['ARGS']['func']
        self.LibMemVersion=''
        self.GlibcVersion=''
        self.size_unit=[]

    def __call__(self):
        self.isExist=os.path.exists(self.path+"/benchmark")
        if (not self.isExist):
            print("Downloading and Configuring GoogleBench")
            subprocess.run(["git","clone","-c","advice.detachedHead=false","-b","v1.8.0","https://github.com/google/benchmark.git"],cwd=self.path)
            subprocess.run(["git","clone","-b","v1.13.x","https://github.com/google/googletest.git","benchmark/googletest"],cwd=self.path)
            subprocess.run(['cmake','-E','make_directory','build'],cwd=self.path+"/benchmark")
            subprocess.run(['cmake','-E','chdir','build','cmake','-DBENCHMARK_DOWNLOAD_DEPENDENCIES=on','-DCMAKE_BUILD_TYPE=Release','../'],cwd=self.path+"/benchmark")
            subprocess.run(['cmake','--build','build','--config','Release'],cwd=self.path+"/benchmark")

        if self.memory_operation == 'u':
            self.bench_name='GooglBench_UnCached'

        self.result_dir = 'out/'+self.bench_name+'/'+self.func + '/' + \
        datetime.datetime.now().strftime("%d-%m-%Y_%H-%M-%S")
        os.makedirs(self.result_dir, exist_ok=False)

        print("Benchmarking of "+str(self.func)+" for size range["+str(self.ranges[0])+"-"+str(self.ranges[1])+"] on "+str(self.bench_name))

        subprocess.run(["g++","-Wno-deprecated-declarations","gbench.cpp","-isystem","benchmark/include","-Lbenchmark/build/src","-lbenchmark","-lpthread","-o","googlebench"],cwd=self.path)
        self.variant="glibc"
        self.gbm_run()
        self.variant="amd"
        self.gbm_run()

        values= subprocess.run(["awk", "/^.*CACHED/ { print $1 }", "gbamd.txt"], cwd=self.result_dir, capture_output=True, text=True).stdout.splitlines()
        self.size_values= [val.split('/')[1] for val in values]
        self.amd_throughput_values = subprocess.run(["grep", "-Eo", r"[0-9]+(\.[0-9]+)?[MG]/s", "gbamd.txt"],cwd=self.result_dir, capture_output=True, text=True).stdout.splitlines()
        self.glibc_throughput_values = subprocess.run(["grep", "-Eo", r"[0-9]+(\.[0-9]+)?[MG]/s", "gbglibc.txt"],cwd=self.result_dir, capture_output=True, text=True).stdout.splitlines()

        #Converting the M/s values to G/s
        self.throughput_converter(self.amd_throughput_values)
        self.throughput_converter(self.glibc_throughput_values)

        self.gains=[]
        for value in range(len(self.size_values)):
                self.gains.append(str(round (((self.amd_throughput_values[value] - self.glibc_throughput_values[value] )/ \
                        self.glibc_throughput_values[value] )*100))+str('%'))

        #Converting sizes to B,KB,MB for reports
        self.data_unit()

        # Open the output file
        with open(self.result_dir+"/"+str(self.bench_name)+"throughput_values.csv", "w",\
                newline="") as output_file:
            writer = csv.writer(output_file)
            # Write the values to the CSV file
            writer.writerow(["Size","Glibc-"+str(self.GlibcVersion,'utf-8').strip(),"LibMem-"+str(self.LibMemVersion,'utf-8').strip(),\
                    "GAINS"])
            for size, gthroughput , athroughput, g  in zip(self.size_unit, \
                    self.glibc_throughput_values,self.amd_throughput_values,self.gains):
                    writer.writerow([size, gthroughput,athroughput,g])

        self.print_result()
        return

    def throughput_converter(self,value):
        for i in range(len(value)):

            if "G/s" in value[i]:
                self.value = float(value[i].strip("G/s"))
                value[i] = self.value
            else:
                self.value = float(value[i].strip("M/s"))
                self.value /= 1000
                value[i] = self.value
        return

    def data_unit(self):
        for x in range(len(self.size_values)):
            if int(self.size_values[x]) >= 1024*1024:
                self.size_unit.append(str(int(self.size_values[x])/(1024*1024))+ " MB")
            elif int(self.size_values[x]) >= 1024:
                self.size_unit.append(str(int(self.size_values[x])/(1024))+ " KB")
            else:
                self.size_unit.append(str(int(self.size_values[x]))+ " B")

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
                print("   ",(self.size[x]).ljust(8)+" :"+\
                    (str(self.gains[x])).rjust(6))


        print("\n*** Test reports copied to directory ["+self.result_dir+"] ***\n")
        return True

    def gbm_run(self):

        if self.variant =="amd":
            self.LibMemVersion = subprocess.check_output("file " + LIBMEM_BIN_PATH + \
                "| awk -F 'so.' '/libaocl-libmem.so/{print $3}'", shell =True)
            env['LD_PRELOAD'] = LIBMEM_BIN_PATH
            print("GBM : Running Benchmark on AOCL-LibMem "+str(self.LibMemVersion,'utf-8').strip())
        else:
            self.GlibcVersion = subprocess.check_output("ldd --version | awk '/ldd/{print $NF}'", shell=True)

            env['LD_PRELOAD'] = ''
            print("GBM : Running Benchmark on GLIBC "+str(self.GlibcVersion,'utf-8').strip())
        # size zero will result in 0 throughput.
        if (self.ranges[0] == 0):
            self.ranges[0] = 1;

        with open(self.result_dir+'/gb'+str(self.variant)+'.txt','w') as g:
            subprocess.run(["taskset", "-c", str(self.core),"numactl","-C"+str(self.core),"./googlebench","--benchmark_counters_tabular=true",str(self.func),str(self.memory_operation),str(self.ranges[0]),str(self.ranges[1]), str(self.iterator)],cwd=self.path,env=env,check=True,stdout =g,stderr=subprocess.PIPE)

        return