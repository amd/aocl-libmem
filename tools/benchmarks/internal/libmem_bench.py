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

supp_funcs = ['memcpy', 'mempcpy', 'memmove', 'memset', 'memcmp']

def measure_latency(mem_func, size_range, alignment, iterator, iterations,\
                                                lib_variant, mode):
    """
    This function measures the latency in performing the string operation of
    a specifc library.
    args:
        mem_func: string function name.
        size_range: data size range.
        alignment: src and dst alignment.
        iterator: expression for iterating over data range.
        iterations: number of iterations for each data size.
        lib_variant: library name(glibc or amd)
    returns: none
    """
    size = size_range[0]
    src_align = str(alignment[0])
    dst_align = str(alignment[1])
    #Global Variables for Graph Plot
    global func
    func = mem_func

    print('>>> Measuring Latency of ['+lib_variant+' '+mem_func+\
            '] for size_range: ['+str(size_range[0])+"-"+str(size_range[1])+\
          '] with src_alignment = '+src_align+', dst_alignment = '+dst_align)
    with open(lib_variant+'_latency_report.csv', 'a+') as latency_report:
        report_writer = csv.writer(latency_report)

        report_writer.writerow(['size','src_align', 'dst_align', 'latency'])
        #set the early binding option for the loader
        os.environ['LD_BIND_NOW'] = '1'
        if lib_variant == 'amd':
            env['LD_PRELOAD'] = '../lib/shared/libaocl-libmem.so'
        else:
            env['LD_PRELOAD'] = ''

        while size <= size_range[1]:
            print('   > Latency measurement for size ['+str(size)+\
                    '] in progress...')
            data = []
            with open(str(size)+".csv", 'a+') as latency_sz:
                for i in range(0, iterations):
                    subprocess.run(['taskset', '-c', '7', \
                            '../tools/benchmarks/internal/libmem_bench_fw', \
                            mem_func, str(size), src_align, dst_align, mode],\
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
            size = eval('size'+' '+ iterator)


def performance_analyser(mem_func, size_range, alignment, iterator, \
                                                iterations, mode):
    """
    This function generates the performance report of amd string function
    against glibc string function.
    args:
        mem_func: string function name.
        size_range: data size range.
        alignment: src and dst alignment.
        iterator: expression for iterating over data range.
        iterations: number of iterations for each data size.
    returns: list
        [perf_min,perf_max]
    """

    measure_latency(mem_func, size_range, alignment, iterator, iterations,\
                                                             'glibc', mode)

    measure_latency(mem_func, size_range, alignment, iterator, iterations,\
                                                              'amd', mode)
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
    parser = argparse.ArgumentParser(prog='bench_libmem', description='This\
                            program will perform data validation, latency \
                            measurement and performance of the specified \
                            string function for a given range of data \
                            lengths and alignments.')
    parser.add_argument("func", help = "string function to be verified",
                            type = str, choices = supp_funcs)
    parser.add_argument("-m", "--mode", help = "type of benchmarking mode:\
                            c - cached, u - un-cached, w - walk, p - page_walk\
                            Default choice is to run un-cached benchmark.",\
                            type = str, default = 'u', \
                            choices = ['c', 'u', 'w', 'p'])
    parser.add_argument("-r", "--range", nargs = 2, help="range of data\
                                lengths to be verified.",
                            type = int, default = [8, 32*1024*1024])
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

    args = parser.parse_args()


    test_result = 'Bench Summary:\n'
    test_status = 'Test Status : '
    status = True

    subprocess.call(['rm *.csv'], shell = True)
    # Analyse performance
    if args.mode in ('c', 'u', 'w', 'p'):
        perf_result = performance_analyser(args.func, args.range, args.align,\
                                   args.iterator, args.iterations, args.mode)
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
    if args.graph in ('l', 'h') and args.mode not in ('v','l') :
        global result
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
