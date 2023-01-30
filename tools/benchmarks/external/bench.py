#!/usr/bin/python3
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

def print_result(result_dir,bench_name):
    input_file = read_csv(result_dir+"/"+str(bench_name)+\
            "throughput_values.csv")
    size = input_file['Size'].values.tolist()
    gains = input_file['GAINS'].values.tolist()
    print("\nBENCHMARK: "+bench_name)
    print("    SIZE".ljust(8)+"     : GAINS")
    print("    ----------------")
    for x in range(len(size)):
        if int(size[x]) >= 1024*1024:
            print("   ",((str(int(size[x])/(1024*1024)))+" MB").ljust(8)+" :"+\
                    (str(gains[x])+"%").rjust(6))
        elif int(size[x]) >= 1024:
            print("   ",((str(int(size[x])/(1024)))+" KB").ljust(8)+" :"+(str\
                    (gains[x])+"%").rjust(6))
        else:
            print("   ",(str(int(size[x])) + " B").ljust(8)+" :"+(str\
                    (gains[x])+"%").rjust(6))

    print("\n*** Test reports copied to directory ["+result_dir+"] ***\n")


def tiny_run(variant,func,ranges,result_dir,path,core):
    if variant =="amd":
        env['LD_PRELOAD'] = '../../../../../lib/shared/libaocl-libmem.so'
        print("LIBMEM:",func)
    else:
        env['LD_PRELOAD'] = ''
        print("GLIBC:",func)

    with open(result_dir+'/'+str(variant)+'.txt', 'w') as f:
        subprocess.run(['taskset', '-c',str(core),'./tinymembench',str(func),\
                str(ranges[0]),str(ranges[1])],cwd=path+"/tinymembench",\
                env=env, stdout=f)

def perf_run(variant,func,ranges,result_dir,core):
    if variant =="amd":
        env['LD_PRELOAD'] = '../lib/shared/libaocl-libmem.so'
        print("LIBMEM:",func)
    else:
        env['LD_PRELOAD'] = ''
        print("GLIBC",func)

    pbm=[]
    size = ranges[0]
    while size <= ranges[1]:
        with open(result_dir+'/'+str(size)+'_'+str(variant)+'.txt','a+') as v:
            for i in range(0,10):
                subprocess.run(['taskset', '-c', str(core),'perf','bench','mem',\
                        str(func),'-s',str(size),'B','-l','10000','-f',\
                        'default'],env=env,stdout =v)
            #Extracting all the Throughput Values
            with open(result_dir+'/'+str(size)+'_'+str(variant)+'.txt', 'r')\
                    as z:
                contents = z.read()
            # Extract the GB/s and MB/s values from the input file
            values = re.findall(r"([\d.]+) (GB|MB)\/sec", contents)
            final_values=[]
            for value in values:
                numeric_value = float(value[0])
                unit = value[1] + '/sec'
                if unit == "MB/sec":
                    converted_value = numeric_value / 1024
                else:
                    converted_value = numeric_value

                final_values.append(converted_value)
            sort=sorted(final_values, reverse =True)
            best = int(len(sort)*0.6)
            sort_best = sort[0:best]
            avg=mean(sort_best)
            pbm.append(avg)
        size = size * 2
    return pbm


supp_funcs = ['memcpy','memset']
def main():
    avaliable_cores = subprocess.check_output("lscpu | grep 'CPU(s):' | \
         awk '{print $2}' | head -n 1", shell=True).decode('utf-8').strip()

    parser = argparse.ArgumentParser(prog='bench', description='This\
                            program will perform the benchmarking')
    parser.add_argument("benchmark", help="select the \
            benchmarking tool for LibMem",
                         type=str, default ="gbm",choices = ["tbm", "gbm","pbm"])
    parser.add_argument("func", help="string function to be verified",
                            type=str, choices = supp_funcs,default="memcpy")
    parser.add_argument("-r", "--range", nargs = 2, help="range of data\
                                lengths to be verified.",
                            type=int, default = [8, 32*1024*1024])

    parser.add_argument("-o","--memory_operation",help="Cached or Un-Cached \
                   Benchmark[SUPPORTED BY only GOOGLE BENCH],default=Cached",
                         type=str,choices =["c","u"],default="c")

    parser.add_argument("-g", "--graph", help="Generates the Latency and \
            Throughput Reports by plotting LibMem vs Glibc graphs with gains,\
            Report",type=str,choices=['l'])

    parser.add_argument("-x", "--core_id", help = "CPU core_id on which \
                            benchmark has to be performed. Default choice of \
                            core-id is 8", type = int, default = 8,
                            choices = range(0, int(avaliable_cores)))

    args = parser.parse_args()

    if not os.path.exists("out"):
        os.mkdir("out")
    global result_dir

    if args.benchmark == 'tbm':
        path="../tools/benchmarks/external/tinybench"
        isExist=os.path.exists(path+'/tinymembench')
        if (not isExist):
            print("Preparing Tinymem benchmark")
            subprocess.run(["git","clone", "https://github.com/ssvb/tinymembench.git"],cwd=path)
            subprocess.run(["make"],cwd=path+"/tinymembench")
            os.system("cp ../tools/benchmarks/external/tinybench/tinymem-bench.c ../tools/benchmarks/external/tinybench/tinymembench/")
            subprocess.run(["gcc","-O2","-o","tinymembench","tinymem-bench.c",\
                    "util.o","asm-opt.o","x86-sse2.o","-lm"],cwd=path+"/tinymembench")

            print('Prepared Tiny Membench')

        result_dir = 'out/' + args.func + '/' + \
        datetime.datetime.now().strftime("%d-%m-%Y_%H-%M-%S")
        os.makedirs(result_dir, exist_ok=False)
        global bench_name
        bench_name='TinyMemBench'

        print("Running [INTERNAL] Tiny Membench ")

        tiny_run("amd",args.func,args.range,result_dir,path,args.core_id)
        tiny_run("glibc",args.func,args.range,result_dir,path,args.core_id)

        # Extract the size values using a regular expression
        with open(result_dir+"/amd.txt", "r") as input_file:
            data = input_file.read()
        size_values = subprocess.run(["sed", "-n", r"s/SIZE: \([0-9]*\) B.*/\1/p", "amd.txt"],cwd =result_dir, capture_output=True, text=True).stdout.splitlines()

        # Extract the throughput values using the sed command
        amd_throughput_values = subprocess.run(["sed", "-n", r"s/.*:\s*\([0-9.]*\) MB\/s.*/\1/p", "amd.txt"],cwd=result_dir, capture_output=True, text=True).stdout.splitlines()
        glibc_throughput_values = subprocess.run(["sed", "-n", r"s/.*:\s*\([0-9.]*\) MB\/s.*/\1/p", "glibc.txt"],cwd=result_dir, capture_output=True, text=True).stdout.splitlines()
        gains = []

        amd = [eval(i) for i in amd_throughput_values]
        glibc = [eval(i) for i in glibc_throughput_values]

        for value in range(len(size_values)):
            gains.append(round (((amd[value] - glibc[value] )/ glibc[value] )*100))

        # Open the output file
        with open(result_dir+'/'+str(bench_name)+"throughput_values.csv",\
                "w", newline="") as output_file:
            writer = csv.writer(output_file)
            # Write the values to the CSV file
            writer.writerow(["Size", "amd_Throughput","glibc_Throughput",\
                    "GAINS"])
            for size, athroughput , gthroughput, g  in zip(size_values, \
                    amd_throughput_values,glibc_throughput_values,gains):
                    writer.writerow([size, athroughput,gthroughput,g])

        print_result(result_dir,bench_name)

    if args.benchmark == "pbm":
        result_dir = 'out/' + args.func + '/' + \
        datetime.datetime.now().strftime("%d-%m-%Y_%H-%M-%S")
        os.makedirs(result_dir, exist_ok=False)

        bench_name='PerfBench'
        """
        Available function variation of memcpy and memset:
        default ... Default memcpy() provided by glibc
        x86-64-unrolled ... unrolled memcpy() in arch/x86/lib/memcpy_64.S
        x86-64-movsq ... movsq-based memcpy() in arch/x86/lib/memcpy_64.S
        x86-64-movsb ... movsb-based memcpy() in arch/x86/lib/memcpy_64.S
        """
        print("Runnig Perf Bench ")

        pbmglibc= perf_run("glibc",args.func,args.range,result_dir,args.core_id)
        pbmamd =  perf_run("amd",args.func,args.range,result_dir,args.core_id)
        s= []
        size =args.range[0]
        while size <= args.range[1]:
            s.append(size)
            size =size *2

        GAINS=[]
        for value in range(len(s)):
                GAINS.append(round (((pbmamd[value] - pbmglibc[value] )/ \
                        pbmglibc[value] )*100))
            # Open the output file
        with open(result_dir+"/"+str(bench_name)+"throughput_values.csv", \
                "w", newline="") as output_file:
            writer = csv.writer(output_file)
            # Write the values to the CSV file
            writer.writerow(["Size", "amd_Throughput","glibc_Throughput",\
                    "GAINS"])
            for size, athroughput , gthroughput, g  in zip(s, pbmamd,pbmglibc\
                    ,GAINS):
                writer.writerow([size, athroughput,gthroughput,g])

        print_result(result_dir,bench_name)

    if args.benchmark =="gbm":
        path="../tools/benchmarks/external/gbench"
        isExist=os.path.exists(path+"/benchmark")
        if not isExist:
            print("Downloading and Configuring GoogleBench")
            subprocess.run(["git","clone", "https://github.com/google/benchmark.git"],cwd=path)
            subprocess.run(["git","clone", "https://github.com/google/googletest.git","benchmark/googletest"],cwd=path)
            subprocess.run(["mkdir","build"],cwd=path+"/benchmark")
            subprocess.run(["cmake","../"],cwd=path+"/benchmark/build")
            subprocess.run(["make"],cwd=path+"/benchmark/build")
            os.system("cp ../tools/benchmarks/external/gbench/gbench.cpp ../tools/benchmarks/external/gbench/benchmark/gbench.cpp")

        if args.memory_operation in ['c','u']:
            result_dir = 'out/' + args.func + '/' + \
            datetime.datetime.now().strftime("%d-%m-%Y_%H-%M-%S")
            os.makedirs(result_dir, exist_ok=False)
            path="../tools/benchmarks/external/gbench/benchmark"
            if args.memory_operation == 'c':
                bench_name='GooglBench_Cached'
            else:
                bench_name='GooglBench_UnCached'

            print("Running GoogleBench")
            print(bench_name)

            subprocess.run(["g++","gbench.cpp","-lbenchmark","-lpthread","-o","bench"],cwd=path)

            env['LD_PRELOAD'] = '../../../../../lib/shared/libaocl-libmem.so'

            with open(result_dir+'/gbamd.txt','w') as f:
                subprocess.run(['taskset', '-c', str(args.core_id),'./bench','--benchmark_counters_tabular=true',str(args.func),str\
                        (args.memory_operation),str(args.range[0]),str\
                        (args.range[1])],cwd=path,env=env,check=True,stdout=f )

            env['LD_PRELOAD'] = ''
            with open(result_dir+'/gbglibc.txt','w') as g:
                subprocess.run(["taskset", "-c", str(args.core_id),"./bench","--benchmark_counters_tabular=true",str(args.func),str\
                        (args.memory_operation),str(args.range[0]),\
                        str(args.range[1])],cwd=path,env=env,check=True,stdout =g)

            if args.func== 'memcpy':
                size_values = subprocess.run(["sed", "-n", r"s/.*cached_memcpy\/\([0-9]\+\).*/\1/p", "gbamd.txt"],cwd =result_dir, capture_output=True, text=True).stdout.splitlines()
            else:
                size_values = subprocess.run(["sed", "-n", r"s/.*cached_memset\/\([0-9]\+\).*/\1/p", "gbamd.txt"],cwd =result_dir, capture_output=True, text=True).stdout.splitlines()
            amd_throughput_values = subprocess.run(["grep", "-Eo", r"[0-9]+\.[0-9]+[MG]/s", "gbamd.txt"],cwd=result_dir, capture_output=True, text=True).stdout.splitlines()
            glibc_throughput_values = subprocess.run(["grep", "-Eo", r"[0-9]+\.[0-9]+[MG]/s", "gbglibc.txt"],cwd=result_dir, capture_output=True, text=True).stdout.splitlines()
            #Converting the G/s values to M/s
            for i in range(len(amd_throughput_values)):
                if "G/s" in amd_throughput_values[i]:
                    value = float(amd_throughput_values[i].strip("G/s"))
                    value *= 1024
                    amd_throughput_values[i] = value
                else:
                    value = float(amd_throughput_values[i].strip("M/s"))
                    amd_throughput_values[i] = value

            for i in range(len(glibc_throughput_values)):
                if "G/s" in glibc_throughput_values[i]:
                    value = float(glibc_throughput_values[i].strip("G/s"))
                    value *= 1024
                    glibc_throughput_values[i] = value
                else:
                    value = float(glibc_throughput_values[i].strip("M/s"))
                    glibc_throughput_values[i] = value

            print(amd_throughput_values)
            print(glibc_throughput_values)
            gains=[]
            for value in range(len(size_values)):
                    gains.append(round (((amd_throughput_values[value] - glibc_throughput_values[value] )/ \
                            glibc_throughput_values[value] )*100))

            # Open the output file
            with open(result_dir+"/"+str(bench_name)+"throughput_values.csv", "w",\
                    newline="") as output_file:
                writer = csv.writer(output_file)
                # Write the values to the CSV file
                writer.writerow(["Size", "amd_Throughput","glibc_Throughput",\
                        "GAINS"])
                for size, athroughput , gthroughput, g  in zip(size_values, \
                        amd_throughput_values,glibc_throughput_values,gains):
                        writer.writerow([size, athroughput,gthroughput,g])

            print_result(result_dir,bench_name)

    if args.graph =='l':
        #Check for req modules
        try:
            import matplotlib.pyplot
            from matplotlib import colors
            import pandas
            import numpy
        except ImportError as e:
            print(e)
            print("FAILED to generate performance Graphs due to MISSING modules")
        else:
            print('Linechart Report Generation in PROGRESS..and may take some time...')
            import graph_generator

if __name__=="__main__":
    main()
