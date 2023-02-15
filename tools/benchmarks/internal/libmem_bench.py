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

class Libmem_Bench:
    def __init__(self, args):
       self.libmem_func = str(args.func)
       self.start_size = args.range[0]
       self.end_size = args.range[1]
       self.src_align = str(args.align[0])
       self.dst_align = str(args.align[1])
       self.iterator = str(args.iterator)
       self.iterations = args.iterations
       self.mode = str(args.mode)
       self.core_id = str(args.core_id)
       self.mixed_bench_marker = 1024 * 1024

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
                env['LD_PRELOAD'] = '../lib/shared/libaocl-libmem.so'
            else:
                env['LD_PRELOAD'] = ''
            size = self.start_size
            while size <= self.end_size:
                print('   > Latency measurement for size ['+str(size)+\
                        '] in progress...')
                data = []
                bench_mode = self.mode
                if self.mode == 'm': # mixed benchmarking
                    bench_mode = 'c'
                    if size >= self.mixed_bench_marker:
                        bench_mode = 'u'

                with open(str(size)+".csv", 'a+') as latency_sz:
                    for i in range(0, self.iterations):
                        subprocess.run(['taskset', '-c', self.core_id, \
                           '../tools/benchmarks/internal/libmem_bench_fw', \
                           self.libmem_func, str(size), self.src_align, \
                           self.dst_align, bench_mode],\
                           stdout=latency_sz, env = env)
                    latency_sz.seek(0)
                    csv_rdr = csv.reader(latency_sz)

                    sort = sorted(csv_rdr, key=lambda x: int(x[1]))
                    best = int(len(sort)*0.6)
                    sort_best = sort[0:best]
                    avg = round(sum(int(x[1]) for x in sort_best)/best,2)
                    data = list(map(int, (sort_best[0])[0:1]))
                    data.append(avg)
                    report_writer.writerow(data)
                    subprocess.run(['rm', str(size)+".csv"])
                size = eval('size'+' '+ self.iterator)


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


def main():
    libmem_funcs = ['memcpy', 'mempcpy', 'memmove', 'memset', 'memcmp']

    avaliable_cores = subprocess.check_output("lscpu | grep 'CPU(s):' | \
         awk '{print $2}' | head -n 1", shell=True).decode('utf-8').strip()

    parser = argparse.ArgumentParser(prog='bench_libmem', description='This\
                            program will perform data validation, latency \
                            measurement and performance of the specified \
                            string function for a given range of data \
                            lengths and alignments.')
    parser.add_argument("func", help = "string function to be verified",
                            type = str, choices = libmem_funcs)
    parser.add_argument("-m", "--mode", help = "type of benchmarking mode:\
                            c - cached, u - un-cached, w - walk, p - page_walk\
                            Default choice is to run cached upto 1MB and \
                            un-cached benchmark for larger sizes(>=1MB).",\
                            type = str, default = 'm', \
                            choices = ['c', 'u', 'w', 'p'])
    parser.add_argument("-r", "--range", nargs = 2, help="range of data\
                                lengths to be verified.",
                            type = int, default = [8, 64 * 1024 * 1024])
    parser.add_argument("-a", "--align", nargs = 2, help = "alignemnt of source\
                                and destination addresses. Default alignment\
                                is 64B for both source and destination.",
                            type = int, default = (64, 64))
    parser.add_argument("-t", "--iterator", help = "iteration pattern for a \
                            given range of data sizes. Default expression\
                            is set to 2x of starting size - '<<1'.",
                            type = str, default = '<<1')
    parser.add_argument("-i", "--iterations", help = "Number of iterations for\
                                performance measurement. Default value is \
                                set to 1000 iterations.",
                            type = int, default = 1000)
    parser.add_argument("-g", "--graph", help = "Generates the Latency and \
                            Throughput reports by plotting LibMem vs Glibc\
                             graphs with gains,\
                            h - Histogram Report , l - Linechart Report\
                            Default choice is Histogram.",
                            type = str, default = 'none', choices = ['h','l'] )
    parser.add_argument("-x", "--core_id", help = "CPU core_id on which \
                            benchmark has to be performed. Default choice of \
                            core-id is 8", type = int, default = 8,
                            choices = range(0, int(avaliable_cores)))

    args = parser.parse_args()
    lbm = Libmem_Bench(args)
    test_result = 'Bench Summary:\n'
    test_status = 'Test Status : '
    status = True

    subprocess.call(['rm *.csv'], shell = True)
    # Analyse performance
    perf_result = lbm.performance_analyser()
    if perf_result[0] < -5:
        status = False
        test_result += ' * Performance Test Failure. Drop of performance'+\
                    ' for one/more data sizes'
    else:
        test_result += ' * Performance Test Success. Performance gains '+\
                            'of range : '+str(perf_result)
        test_result += '. Refer to <perf_report.csv> for more details.\n'

    env['LD_PRELOAD'] = ''

    result_dir = 'out/' + args.func + '/' + \
                datetime.datetime.now().strftime("%d-%m-%Y_%H-%M-%S")
    os.makedirs(result_dir, exist_ok=False)
    subprocess.call(['mv *.csv '+result_dir], shell=True)
    #generate test configuration
    test_config = 'string_function :'+args.func+'\ndata_range[start, end]: '+\
            str(args.range)+'\nalignment(src, dst) :'+str(args.align)+\
            '\niterator : '+str(args.iterator)+'\niterations : '+\
            str(args.iterations)

    with open(result_dir+'/test.config', 'a+') as config_file:
        config_file.write(test_config)

    print(">>> Completed running all tests.\n")

    if status is True:
        test_status += 'SUCCESS\n'
    else:
        test_status += 'FAILURE\n'

    print(test_status,test_result)

    # Generate Graph
    if args.graph in ('l', 'h'):
        global func
        global result
        func = args.func
        result = result_dir
        #Check for req modules
        try:
            import matplotlib.pyplot
            from matplotlib import colors
            import pandas
            import numpy
        except ImportError as e:
            print(e)
            print("FAILED to generate graphs due to MISSING modules")
        else:
            if args.graph == 'l':
                print('Linechart graph generation in PROGRESS...')
                import linechart
            elif args.graph == 'h':
                print('Histogram report generation in PROGRESS...')
                import histogram

    print("\n*** Test reports copied to directory ["+result_dir+"] ***\n")

    with open(result_dir+'/test_summary.log', 'a+') as summary:
        summary.write(test_status)
        summary.write(test_result)


if __name__=="__main__":
    main()
