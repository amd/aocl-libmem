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
import csv
import argparse
import datetime
import pandas as pd
class LBM:
    def __init__(self,**kwargs):
        self.ARGS = kwargs
        self.MYPARSER = self.ARGS["ARGS"]
        self.libmem_func = self.MYPARSER['ARGS']['func']
        self.range = self.MYPARSER['ARGS']['range']
        self.start_size = self.MYPARSER['ARGS']['range'][0]
        self.end_size = self.MYPARSER['ARGS']['range'][1]
        self.align = self.MYPARSER['ARGS']['align']
        self.src_align = str(self.MYPARSER['ARGS']['align'][0])
        self.dst_align = str(self.MYPARSER['ARGS']['align'][1])
        self.iterator = str(self.MYPARSER['ARGS']['iterator'])
        self.iterations = self.MYPARSER['ARGS']['iterations']
        self.mode = str(self.MYPARSER['ARGS']['mode'])
        self.core_id = str(self.MYPARSER['ARGS']['core_id'])
        self.mixed_bench_marker = 1024 * 1024
        self.bench_mode=[]
        self.bench_name='LibMem Bench'

    def measure_latency(self, lib_variant):
        """
        This function measures the latency in performing the string operation
        of a specifc libmem_function and writes to csv files.
        args:
            lib_variant: library name(glibc or amd)
        returns: none
        """

        print('>>> Measuring Latency of ['+lib_variant+' '+self.libmem_func+\
                '] for size_range: ['+str(self.start_size)+"-"+\
                str(self.end_size)+'] with src_alignment = '+ \
                str(self.src_align)+', dst_alignment = '+str(self.dst_align))
        with open(lib_variant+'_latency_report.csv', 'a+') as latency_report:
            report_writer = csv.writer(latency_report)

            report_writer.writerow(['size', 'latency'])
            #set the early binding option for the loader
            os.environ['LD_BIND_NOW'] = '1'
            if lib_variant == 'amd':
                LibMemVersion = subprocess.check_output("file ../lib/shared/libaocl-libmem.so \
                | awk -F 'so.' '/libaocl-libmem.so/{print $3}'", shell =True)
                env['LD_PRELOAD'] = '../lib/shared/libaocl-libmem.so'
                print("LBM : Running Benchmark on AOCL-LibMem "+str(LibMemVersion,'utf-8').strip())
            else:
                GlibcVersion = subprocess.check_output("ldd --version | awk '/ldd/{print $NF}'", shell=True)
                env['LD_PRELOAD'] = ''
                print("LBM : Running Benchmark on GLIBC "+str(GlibcVersion,'utf-8').strip())
            self.size = self.start_size
            self.bench_mode = self.mode
            if self.mode == 'm': # mixed benchmarking
                self.bench_mode = 'c'

            data = []
            if self.mode == 'm' and self.size >= self.mixed_bench_marker:
                self.bench_mode = 'u'

            total = None
            file_name = lib_variant+"_raw.csv"
            with open(file_name, 'a+') as latency_sz:
                for i in range(0, self.iterations):
                    subprocess.run(['taskset', '-c', self.core_id,"numactl","-C"+str(self.core_id), \
                    '../tools/benchmarks/internal/libmem_bench_fw', \
                    self.libmem_func, str(self.start_size), str(self.end_size),self.src_align, \
                    self.dst_align,self.bench_mode, self.iterator],\
                    stdout=latency_sz, env = env)

            average_of_top_60_percent=[]
            df= pd.read_csv(file_name)

            #Removin the last NULL column
            df = df.iloc[:,:-1]
            #TOP 60% value
            for indx in df.columns:
                sorted_column = df[indx].sort_values(ascending = True)
                top_60_percent_index = int(len(sorted_column) * 0.6)
                average_of_top_60_percent.append( round(sorted_column[:top_60_percent_index].mean()))

            pos=0
            iterator = "<< 1"
            if  int(self.iterator):
                iterator = "+"+(self.iterator)
            size = int(self.start_size)
            while size <= int(self.end_size):
                data = [size, average_of_top_60_percent[pos]]
                pos=pos+1
                report_writer.writerow(data)
                size = eval('size'+' '+ iterator)

    def performance_analyser(self):
        """
        This function generates the performance report of amd string function
        against glibc string function.
        args: none
        returns: list
            [perf_min,perf_max]
        """
        self.measure_latency('glibc')
        self.measure_latency('amd')

        perf_data = []
        with open('glibc_latency_report.csv', 'r') as glibc,\
             open('amd_latency_report.csv', 'r') as amd,\
             open('perf_report.csv','a+') as perf:
            glibc_reader = csv.reader(glibc)
            amd_reader = csv.reader(amd)
            perf_writer = csv.writer(perf)

            #Omit headers
            amd_row = next(amd_reader, None)
            glibc_row = next(glibc_reader, None)
            perf_writer.writerow([glibc_row[0], 'performance gains(%)'])

            #read first data line
            glibc_row = next(glibc_reader, None)
            amd_row = next(amd_reader, None)
            print("    SIZE".ljust(8)+"     : GAINS")
            print("    ----------------")
            while glibc_row and amd_row:
                res = round(((float(glibc_row[1])/float(amd_row[1]))-1)*100)
                perf_data.append(res)
                perf_writer.writerow([glibc_row[0], res])
                if int(glibc_row[0]) >= 1024*1024:
                    print("   ",((str(int(glibc_row[0])/(1024*1024)))+" MB").\
                                    ljust(8)+" :"+(str(res)+"%").rjust(6))
                elif int(glibc_row[0]) >= 1024:
                    print("   ",((str(int(glibc_row[0])/(1024)))+" KB").\
                                    ljust(8)+" :"+(str(res)+"%").rjust(6))
                else:
                    print("   ",(glibc_row[0] + " B").ljust(8)+" :"+\
                                                (str(res)+"%").rjust(6))
                glibc_row = next(glibc_reader, None)
                amd_row = next(amd_reader, None)

        return [min(perf_data),max(perf_data)]


    def __call__(self):
        test_result = 'Bench Summary:\n'
        test_status = 'Test Status : '
        status = True

        subprocess.call(['rm *.csv'], shell = True)
        # Analyse performance
        print("Benchmarking of "+str(self.libmem_func)+" for size range["+str(self.start_size)+"-"+str(self.end_size)+"] on "+str(self.bench_name))
        perf_result = self.performance_analyser()
        if perf_result[0] < -5:
            status = False
            test_result += ' * Performance Test Failure. Drop of performance'+\
                        ' for one/more data sizes'
        else:
            test_result += ' * Performance Test Success. Performance gains '+\
                                'of range : '+str(perf_result)
            test_result += '. Refer to <perf_report.csv> for more details.\n'

        env['LD_PRELOAD'] = ''

        self.result_dir = 'out/' + self.libmem_func + '/' + \
                    datetime.datetime.now().strftime("%d-%m-%Y_%H-%M-%S")
        os.makedirs(self.result_dir, exist_ok=False)
        subprocess.call(['mv *.csv '+self.result_dir], shell=True)
        #generate test configuration
        test_config = 'string_function :'+self.libmem_func+'\ndata_range[start, end]: '+\
                str(self.range)+'\nalignment(src, dst) :'+str(self.align)+\
                '\niterator : '+str(self.iterator)+'\niterations : '+\
                str(self.iterations)

        with open(self.result_dir+'/test.config', 'a+') as config_file:
            config_file.write(test_config)

        print(">>> Completed running all tests.\n")

        if status is True:
            test_status += 'SUCCESS\n'
        else:
            test_status += 'FAILURE\n'

        print(test_status,test_result)

        print("\n*** Test reports copied to directory ["+self.result_dir+"] ***\n")

        with open(self.result_dir+'/test_summary.log', 'a+') as summary:
            summary.write(test_status)
            summary.write(test_result)

        return

