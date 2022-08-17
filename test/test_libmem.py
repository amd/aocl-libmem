#!/usr/bin/python3
"""
 Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

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

supp_funcs = ['memcpy', 'mempcpy', 'memmove', 'memset']

def data_validator(mem_func, size_range, alignment, iterator):
    """
    This function validates the data integrity of a string
    function.
    args:
        mem_func: string function name.
        size_range: data size range.
        alignment: src and dst alignment.
        iterator: expression for iterating over data range.
    returns: string
        'Success' / 'Failure'
    """
    size = size_range[0]
    src_align = str(alignment[0])
    dst_align = str(alignment[1])
    print('>>> Data validaton of [amd '+mem_func+'] for size_range: ['\
          +str(size_range[0])+"-"+str(size_range[1])+'] with src_alignment = '\
          +src_align+', dst_alignment = '+dst_align)

    env['LD_PRELOAD'] = '../lib/libaocl-libmem.so'

    validation_status = 'Success'
    with open("validation_report.csv", 'a+') as v_result:
        while size <= size_range[1]:
            print('   > Validation for size ['+str(size)+'] in progress...')
            subprocess.run([mem_func+'/test_'+mem_func, 'v',str(size),\
                    src_align, dst_align], stdout=v_result, env = env, check=True)
            size = eval('size'+' '+ iterator)
        print(v_result.read())
    if 'ERROR' in open("validation_report.csv", 'r').read():
        validation_status = 'Failure'

    return validation_status


def measure_latency(mem_func, size_range, alignment, iterator, iterations,\
                                                lib_variant):
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

    print('>>> Measuring Latency of ['+lib_variant+' '+mem_func+\
            '] for size_range: ['+str(size_range[0])+"-"+str(size_range[1])+\
          '] with src_alignment = '+src_align+', dst_alignment = '+dst_align)
    with open(lib_variant+'_latency_report.csv', 'a+') as latency_report:
        report_writer = csv.writer(latency_report)

        report_writer.writerow(['size','src_align', 'dst_align', 'latency'])
        #set the early binding option for the loader
        os.environ['LD_BIND_NOW'] = '1'
        if lib_variant == 'amd':
            env['LD_PRELOAD'] = '../lib/libaocl-libmem.so'
        else:
            env['LD_PRELOAD'] = ''

        while size <= size_range[1]:
            print('   > Latency measurement for size ['+str(size)+\
                    '] in progress...')
            data = []
            with open(str(size)+".csv", 'a+') as latency_sz:
                for i in range(0, iterations):
                    subprocess.run(['taskset', '-c', '7', mem_func+'/test_'+\
                            mem_func, 'l', str(size), src_align, dst_align],\
                            stdout=latency_sz, env = env)
                latency_sz.seek(0)
                csv_rdr = csv.reader(latency_sz)

                sort = sorted(csv_rdr, key=lambda x: int(x[3]))
                best = int(len(sort)*0.6)
                sort_best = sort[0:best]
                avg = round(sum(int(x[3]) for x in sort_best)/best,2)
                data = list(map(int, (sort_best[0])[0:3]))
                data.append(avg)
                report_writer.writerow(data)
                subprocess.run(['rm', str(size)+".csv"])
            size = eval('size'+' '+ iterator)


def performance_analyser(mem_func, size_range, alignment, iterator, iterations):
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
                                                                    'glibc')

    measure_latency(mem_func, size_range, alignment, iterator, iterations,\
                                                                    'amd')
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
        perf_writer.writerow([glibc_row[0], glibc_row[1], glibc_row[2],\
                                                'performance gains(%)'])

        #read first data line
        glibc_row = next(glibc_reader, None)
        amd_row = next(amd_reader, None)
        print("    SIZE".ljust(8)+"     : GAINS")
        print("    ----------------")
        while glibc_row and amd_row:
            res = round(((float(glibc_row[3])/float(amd_row[3]))-1)*100)
            perf_data.append(res)
            perf_writer.writerow([glibc_row[0], glibc_row[1], glibc_row[2],\
                                                       res])
            if int(glibc_row[0]) >= 1024*1024:
                print("   ",((str(int(glibc_row[0])/(1024*1024)))+" MB").ljust(8)+" :"+(str(res)+"%").rjust(6))
            elif int(glibc_row[0]) >= 1024:
                print("   ",((str(int(glibc_row[0])/(1024)))+" KB").ljust(8)+" :"+(str(res)+"%").rjust(6))
            else:
                print("   ",(glibc_row[0] + " B").ljust(8)+" :"+(str(res)+"%").rjust(6))
            glibc_row = next(glibc_reader, None)
            amd_row = next(amd_reader, None)

    return [min(perf_data),max(perf_data)]


def main():
    parser = argparse.ArgumentParser(prog='test_libmem', description='This\
                            program will perform data validation, latency \
                            measurement and performance of the specified \
                            string function for a given range of data \
                            lengths and alignments.')
    parser.add_argument("func", help="string function to be verified",
                            type=str, choices = supp_funcs)
    parser.add_argument("-m", "--mode", help="Testing mode v-data validation,\
                         l - latency measurement, p - perfromance analysis\
                         Default choice is to run validation and performance.",
                            type=str, default = 'a', choices = ['v', 'l', 'p'])
    parser.add_argument("-l", "--lib", help="String library against which \
                            testing should be done.\
                            g - Glibc, a - AMD. default choice is AMD library.",
                            type=str, default = 'a', choices = ['g', 'a'])
    parser.add_argument("-r", "--range", nargs = 2, help="range of data\
                                lengths to be verified.",
                            type=int, default = [8, 32*1024*1024])
    parser.add_argument("-a", "--align", nargs = 2, help="alignemnt of source\
                                and destination addresses. Default alignment\
                                is 32B for both source and destination.",
                            type=int, default = (32, 32))
    parser.add_argument("-t", "--iterator", help="iteration pattern for a \
                            given range of data sizes. Default expression\
                            is set to 2x of starting size - '<<1'.",
                            type=str, default='<<1')
    parser.add_argument("-i", "--iterations", help="Number of iterations for\
                                performance measurement. Default value is \
                                set to 1000 iterations.",
                            type=int, default=1000)

    args = parser.parse_args()


    library = 'amd'
    if args.lib == 'a':
        library = 'amd'
    elif args.lib == 'g':
        library = 'glibc'

    test_result = 'Test Summary:\n'
    test_status = 'Test Status : '
    status = True

    # Validate data
    if args.mode in ('v','a'):
        validation = data_validator(args.func, args.range, args.align, args.iterator)
        if validation == 'Failure':
            status = False
        test_result += ' * Data Validation '+ validation +\
                '. Refer to <validation_report.csv> for more details.\n'

    # Analyse performance
    if args.mode in ('p','a'):
        perf_result = performance_analyser(args.func, args.range, args.align,\
                                        args.iterator, args.iterations)
        if perf_result[0] < -5:
            status = False
            test_result += ' * Performance Test Failure. Drop of performance'+\
                    ' for one/more data sizes'
        else:
            test_result += ' * Performance Test Success. Performance gains of'+\
                            ' range : '+str(perf_result)
        test_result += '. Refer to <perf_report.csv> for more details.\n'

    # Measure Latency
    if args.mode == 'l':
        measure_latency(args.func, args.range, args.align, args.iterator,\
                                                args.iterations, library)
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

    print("\n*** Test reports copied to directory ["+result_dir+"] ***\n")

    with open(result_dir+'/test_summary.log', 'a+') as summary:
        summary.write(test_status)
        summary.write(test_result)


if __name__=="__main__":
    main()
