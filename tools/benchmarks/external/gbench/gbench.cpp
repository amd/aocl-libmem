/* Copyright (C) 2023-24 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <benchmark/benchmark.h>
#include <cstring>
#include <iostream>
#include <string>
#include <cstddef>
#include <unistd.h> //for Page_size
#include <memory>
#include <immintrin.h>
#include <math.h>
#include <algorithm>

#define MIN_PRINTABLE_ASCII     32
#define MAX_PRINTABLE_ASCII     127
#define NULL_TERM_CHAR          '\0'

#ifdef AVX512_FEATURE_ENABLED
    #define VEC_SIZE            64
#else
    #define VEC_SIZE            32
#endif

#define CL_SIZE                 64
#define CACHELINE_MULTIPLE      6

#define CLFLUSHOPT(addr, cacheline_iters) \
    _mm_clflushopt((void *)((uintptr_t)(addr) + (cacheline_iters) * CL_SIZE))

enum MemoryMode {
    CACHED,
    UNCACHED
};

void Bench_Result(benchmark::State& state)
{
state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);
}

#define REGISTER_MEM_FUNCTION(name) \
    { #name, (void *(*)(void *, const void *, size_t))name }

#define REGISTER_MISC_MEM_FUNCTION(name) \
    { #name, (void *(*)(void *, int, size_t))name }

#define REGISTER_STR_FUNCTION(name) \
    { #name, (void *(*)(char *,const char *))name }

#define REGISTER_STR_N_FUNCTION(name) \
    { #name, (void *(*)(char *,const char *, size_t))name }

//Functions with diff signatrues
struct MemData {
    const char *functionName;
    void *(*functionPtr)(void *dest, const void *src, size_t count);
};

struct Misc_MemData {
    const char *functionName;
    void *(*functionPtr)(void*src, int value, size_t size);
};

const char* my_strchr(const char* s, int c) {
    return strchr(s, c);
}
struct Misc_StrData {
    const char *functionName;
    const char* (*functionPtr)(char *src, int value);
};

struct StrData {
    const char *functionName;
    void *(*functionPtr)(char *dest, const char *src);
};

char* my_strstr(char* haystack, const char* needle) {
    return strstr(haystack, needle);
}

struct substr{
    const char* functionName;
    char* (*functionPtr)(char*, const char*);
};

struct Str_n_Data {
    const char *functionName;
    void *(*functionPtr)(char *dest, const char *src, size_t n);
};

struct Strlen_Data {
    const char *functionName;
    void *(*functionPtr)(const char *src);
};

class MemoryCopyFixture {
public:
    void SetUp(size_t size, char alignment) {
        int page_size;
        size_t SIZE;
        unsigned int offset;
        uint32_t src_alnmnt, dst_alnmnt;
        switch(alignment)
        {
            case 'p': //Page-Aligned allocation
             {  page_size = sysconf(_SC_PAGESIZE);
                SIZE = size + page_size;
                src = new char[SIZE];
                dst = new char[SIZE];
                src_alnd = src;
                std::align(page_size, size, src_alnd, SIZE);
                dst_alnd = dst;
                std::align(page_size, size, dst_alnd, SIZE);
                break;
             }
            case 'v': //Vector-Aligned allocation
              {
                SIZE = size + VEC_SIZE;
                src = new char[SIZE];
                dst = new char[SIZE];
                src_alnd = src;
                dst_alnd = dst;

                std::align(VEC_SIZE, size, src_alnd, SIZE);
                std::align(VEC_SIZE, size, dst_alnd, SIZE);
                break;
              }
            case 'c': //Cache-Aligned allocation
              {
                SIZE = size + CL_SIZE;
                src = new char[SIZE];
                dst = new char[SIZE];
                src_alnd = src;
                dst_alnd = dst;

                std::align(CL_SIZE, size, src_alnd, SIZE);
                std::align(CL_SIZE, size, dst_alnd, SIZE);
                break;
              }
            case 'u': //Unaligned allocation
              { src_alnmnt = rand() % (VEC_SIZE -1) +1; // Alignment [1 - 31,63]
                dst_alnmnt = rand() % (VEC_SIZE -1) +1;

                src = new char[size + src_alnmnt];
                offset = (uint64_t)src & (src_alnmnt-1);
                src_alnd = src+src_alnmnt-offset;

                dst = new char[size + dst_alnmnt];
                offset = (uint64_t)dst & (dst_alnmnt-1);
                dst_alnd = dst+dst_alnmnt-offset;
                break;
              }
            default: //Random allocation
              {
                src = new char[size];
                dst = new char[size];
                src_alnd = src;
                dst_alnd = dst;
              }
        }

        //chck for buffer allocation
        if (src == nullptr || dst == nullptr)
        {
            std::cerr<< "Memory Allocation Failed!\n";
            exit(-1);
        }
        memset(src_alnd,'$',size);
        memcpy(dst_alnd,src_alnd,size);
    }

    void TearDown() {
        delete[] src;
        delete[] dst;
    }

    template <typename MemFunction, typename... Args>
    void memRunBenchmark(benchmark::State& state, MemoryMode mode, char alignment, MemFunction func, const char* name, Args... args) {
        int size = state.range(0);

        SetUp(size, alignment);
        if(mode == CACHED)
        {
            for (auto _ : state) {
                benchmark::DoNotOptimize(func(dst_alnd, src_alnd, size));
            }
        }
        else
        {
            for (auto _ : state) {
                state.PauseTiming();
                uint32_t cacheline_iters= size >> CACHELINE_MULTIPLE;
                do
                {
                    CLFLUSHOPT(src_alnd, cacheline_iters);
                    CLFLUSHOPT(dst_alnd, cacheline_iters);
                }while (cacheline_iters--);

                state.ResumeTiming();
                benchmark::DoNotOptimize(func((char*)dst_alnd, (char*)src_alnd, size));
            }
        }
        TearDown();

        Bench_Result(state);
    }

    template <typename MemFunction, typename... Args>
    void Misc_memRunBenchmark(benchmark::State& state, MemoryMode mode, char alignment, MemFunction func, const char* name, Args... args) {
        int size = state.range(0);
        SetUp(size, alignment);
        if(mode == CACHED)
        {
            for (auto _ : state) {
                benchmark::DoNotOptimize(func(src_alnd, 'x', size));
            }
        }
        else
        {
            for (auto _ : state) {
                state.PauseTiming();
                uint32_t cacheline_iters = size >> CACHELINE_MULTIPLE;
                do
                {
                    CLFLUSHOPT(src_alnd, cacheline_iters);
                }while (cacheline_iters--);
                state.ResumeTiming();
                benchmark::DoNotOptimize(func(src_alnd, 'x', size));
            }
        }
        TearDown();
        Bench_Result(state);
    }

protected:
    char* src;
    char* dst;
    void* src_alnd;
    void* dst_alnd;
};

template <typename MemFunction, typename... Args>
void runMemoryBenchmark(benchmark::State& state,  MemoryMode mode, char alignment, MemFunction func, const char* name, Args... args) {
    MemoryCopyFixture fixture;
    fixture.memRunBenchmark(state, mode, alignment, func, name, args...);
}
template <typename MemFunction, typename... Args>
void runMiscMemoryBenchmark(benchmark::State& state,  MemoryMode mode, char alignment, MemFunction func, const char* name, Args... args) {
    MemoryCopyFixture fixture;
    fixture.Misc_memRunBenchmark(state, mode, alignment, func, name, args...);
}

//For string functions dst_buffer is 2* size (for handling strcat and buffer overflow)
class StringFixture {
public:
    void SetUp(size_t size, char alignment) {
         int page_size;
        size_t SIZE;
        unsigned int offset;
        uint32_t src_alnmnt, dst_alnmnt;
        switch(alignment)
        {
            case 'p': //Page-Aligned allocation
             {  page_size = sysconf(_SC_PAGESIZE);
                SIZE = size + page_size;
                src = new char[SIZE];
                dst = new char[2 * SIZE];
                src_alnd = src;
                std::align(page_size, size, src_alnd, SIZE);
                dst_alnd = dst;
                std::align(page_size, size, dst_alnd, SIZE);
                break;
             }
            case 'v': //Vector-Aligned allocation
              {
                SIZE = size + VEC_SIZE;
                src = new char[SIZE];
                dst = new char[2 * SIZE];
                src_alnd = src;
                dst_alnd = dst;

                std::align(VEC_SIZE, size, src_alnd, SIZE);
                std::align(VEC_SIZE, size, dst_alnd, SIZE);
                break;
              }
            case 'c': //Cache-Aligned allocation
              {
                SIZE = size + CL_SIZE;
                src = new char[SIZE];
                dst = new char[2 * SIZE];
                src_alnd = src;
                dst_alnd = dst;

                std::align(CL_SIZE, size, src_alnd, SIZE);
                std::align(CL_SIZE, size, dst_alnd, SIZE);
                break;
              }
            case 'u': //Unaligned allocation
              { src_alnmnt = rand() % (VEC_SIZE -1) +1; // Alignment [1 - 31,63]
                dst_alnmnt = rand() % (VEC_SIZE -1) +1;

                src = new char[size + src_alnmnt];
                offset = (uint64_t)src & (src_alnmnt-1);
                src_alnd = src+src_alnmnt-offset;

                dst = new char[2 * size + dst_alnmnt];
                offset = (uint64_t)dst & (dst_alnmnt-1);
                dst_alnd = dst+dst_alnmnt-offset;
                break;
              }
            default: //Random allocation
              {
                src = new char[size];
                dst = new char[2 * size];
                src_alnd = src;
                dst_alnd = dst;
              }
        }
        //chck for buffer allocation
        if (src == nullptr || dst == nullptr)
        {
            std::cerr<< "Memory Allocation Failed!\n";
            exit(-1);
        }
        memset(src_alnd, 'x', size);

        *((char*)src_alnd + size -1) = NULL_TERM_CHAR;
        strcpy((char*)dst_alnd,(char*)src_alnd);
    }

    void TearDown() {
        delete[] src;
        delete[] dst;
    }

    void string_setup(std::string& accept, int size, std::string& s)
    {
        size_t accept_len = ceil(sqrt(size));
        accept = generate_random_string(accept_len);
        for (size_t i=0 ; i < accept_len; i++)
        {
            s += accept.substr(0,i);  // All the substrings of accept
        }

        //Using the permutations of accept
        do
        {
            s += accept;
        }while( s.length() <= size && std::next_permutation(accept.begin(), accept.end()));
    }

    std::string generate_random_string(size_t length) {
        std::string random_string;
        for (int i = 0; i < length; ++i) {
            random_string += MIN_PRINTABLE_ASCII + (i % (MAX_PRINTABLE_ASCII - MIN_PRINTABLE_ASCII));
        }
        return random_string;
    }


    template <typename MemFunction, typename... Args>
    void strRunBenchmark(benchmark::State& state, MemoryMode mode, char alignment, MemFunction func, const char* name, Args... args) {
        int size = state.range(0);
        SetUp(size, alignment);
        if(mode == CACHED)
        {
            if (name == "strcat")
            {
                for (auto _ : state) {
                    *((char*)dst_alnd + size -1) = NULL_TERM_CHAR;
                    benchmark::DoNotOptimize(func((char*)dst_alnd, (char*)src_alnd));
                }
            }
            else if (name == "strspn")
            {
                std::string s;
                std::string accept;

                string_setup(accept, size, s);
                for (auto _ : state) {
                    benchmark::DoNotOptimize(strspn(s.c_str(), accept.c_str()));
                }

            }
            else
            {
                for (auto _ : state) {
                    benchmark::DoNotOptimize(func((char*)dst_alnd, (char*)src_alnd));
                }
            }
        }
        else
        {
            if (name == "strspn")
            {
                std::string s;
                std::string accept;
                string_setup(accept, size, s);

                for (auto _ : state) {
                    state.PauseTiming();
                    uint32_t cacheline_iters= s.length() >> CACHELINE_MULTIPLE;
                    do
                    {
                        CLFLUSHOPT(s.data(), cacheline_iters);
                    }while (cacheline_iters--);

                    cacheline_iters= accept.length() >> CACHELINE_MULTIPLE;
                    do
                    {
                        CLFLUSHOPT(accept.data(), cacheline_iters);
                    }while (cacheline_iters--);
                    state.ResumeTiming();
                    benchmark::DoNotOptimize(strspn(s.c_str(), accept.c_str()));
                }
            }

            else
            {
                for (auto _ : state) {
                    state.PauseTiming();
                    if (name == "strcat")
                    {
                        *((char*)dst_alnd + size -1) = NULL_TERM_CHAR;
                    }
                    uint32_t cacheline_iters = size >> CACHELINE_MULTIPLE;
                    do
                    {
                        CLFLUSHOPT(src_alnd, cacheline_iters);
                        CLFLUSHOPT(dst_alnd, cacheline_iters);
                    }while (cacheline_iters--);
                    state.ResumeTiming();
                    benchmark::DoNotOptimize(func((char*)dst_alnd, (char*)src_alnd));
                }
            }
        }
        TearDown();
    Bench_Result(state);
    }


    template <typename MemFunction, typename... Args>
    void strlenRunBenchmark(benchmark::State& state, MemoryMode mode,char alignment, MemFunction func, const char* name, Args... args) {
        int size = state.range(0);
        SetUp(size, alignment);
        if(mode == CACHED)
        {
            for (auto _ : state) {
            benchmark::DoNotOptimize(func((char*)src_alnd));
            }
        }
        else
        {
            for (auto _ : state) {
                state.PauseTiming();
                uint32_t cacheline_iters = size >> CACHELINE_MULTIPLE;
                do
                {
                    CLFLUSHOPT(src_alnd, cacheline_iters);
                }while (cacheline_iters--);
                state.ResumeTiming();
                benchmark::DoNotOptimize(func((char*)src_alnd));
            }
        }
        TearDown();
    Bench_Result(state);
    }

    template <typename MemFunction, typename... Args>
    void substrRunBenchmark(benchmark::State& state, MemoryMode mode,char alignment, MemFunction func, const char* name, Args... args) {
        int size = state.range(0);
        if (mode == CACHED)
        {
            SetUp(size, alignment);
            std::string haystack_best, haystack_nomatch, haystack_avg;
            std::string needle;

            size_t needle_len = ceil(sqrt(size));
            needle = generate_random_string(needle_len);

            //Match at the beginning:
            haystack_best += needle;
            for (size_t i = 0; i < size - needle_len; i++)
                haystack_best += 'A' + rand() % 26;

            //NO - Match case:
            for (size_t i = 0; i < size; i++)
                haystack_nomatch += 'A' + rand() % 26;

            //Match at the end:
            for (size_t i = 0 ; i < needle_len; i++)
            {
               haystack_avg += needle.substr(0,i);  //All the substrings of needle, except the needle itself
                if (haystack_avg.length() <= size)
                {
                    haystack_avg += 'A' + rand() % 26;
                }
            }
            while (haystack_avg.length() + needle_len <= size) {
                haystack_avg += generate_random_string(1);  //Padding with dummy chars
            }
            haystack_avg += needle; //needle is the last part of haystack

            for (auto _ : state) {
                benchmark::DoNotOptimize(strstr(haystack_best.c_str(), needle.c_str()));
                benchmark::DoNotOptimize(strstr(haystack_nomatch.c_str(), needle.c_str()));
                benchmark::DoNotOptimize(strstr(haystack_avg.c_str(), needle.c_str()));
            }
            TearDown();
        }
        else
        {
            for (auto _ : state) {
                state.PauseTiming();
                SetUp(size, alignment);
                std::string haystack;
                std::string needle;
                size_t needle_len = ceil(sqrt(size));
                needle = generate_random_string(needle_len);
                for (size_t i=0; i < needle_len; i++)
                {
                    haystack += needle.substr(0,i);
                    if(haystack.length() <= size)
                    {
                        haystack += 'A' + rand() % 26;
                    }
                }
                while (haystack.length() + needle_len <= size) {
                    haystack += generate_random_string(1);
                }
                haystack += needle;

                uint32_t cacheline_iters= haystack.length() >> CACHELINE_MULTIPLE;
                do
                {
                    CLFLUSHOPT(haystack.data(), cacheline_iters);
                }while (cacheline_iters--);

                cacheline_iters= needle.length() >> CACHELINE_MULTIPLE;
                do
                {
                    CLFLUSHOPT(needle.data(), cacheline_iters);
                }while (cacheline_iters--);

                state.ResumeTiming();

                benchmark::DoNotOptimize(strstr(haystack.c_str(), needle.c_str()));
                state.PauseTiming();
                TearDown();
                state.ResumeTiming();
            }
        }
    Bench_Result(state);
    }

    template <typename MemFunction, typename... Args>
    void str_n_RunBenchmark(benchmark::State& state, MemoryMode mode, char alignment, MemFunction func, const char* name, Args... args) {
        int size = state.range(0);
        SetUp(size, alignment);
        if  (mode == CACHED)
        {
            if (name == "strncat")
            {
                for (auto _ : state) {
                    *((char*)dst_alnd + size -1) = NULL_TERM_CHAR;
                    benchmark::DoNotOptimize(func((char*)dst_alnd, (char*)src_alnd, size));
                }
            }
            else
            {
                for (auto _ : state) {
                    benchmark::DoNotOptimize(func((char*)dst_alnd, (char*)src_alnd, size));
                }
            }
        }
        else
        {

            for (auto _ : state) {
                state.PauseTiming();
                if (name == "strncat")
                {
                    *((char*)dst_alnd + size -1) = NULL_TERM_CHAR;
                }
                uint32_t cacheline_iters = size >> CACHELINE_MULTIPLE;
                do
                {
                    CLFLUSHOPT(src_alnd, cacheline_iters);
                    CLFLUSHOPT(dst_alnd, cacheline_iters);
                }while (cacheline_iters--);
                state.ResumeTiming();
                benchmark::DoNotOptimize(func((char*)dst_alnd, (char*)src_alnd, size));
            }
        }
        TearDown();
        Bench_Result(state);
    }
    template <typename MemFunction, typename... Args>
    void strchrRunBenchmark(benchmark::State& state, MemoryMode mode, char alignment, MemFunction func, const char* name, Args... args) {
        int size = state.range(0);
        SetUp(size, alignment);
        if  (mode == CACHED)
        {
            for (auto _ : state) {
                benchmark::DoNotOptimize(func((char*)src_alnd,'X'));
            }

        }
        else
        {
            for (auto _ : state) {
                state.PauseTiming();
                uint32_t cacheline_iters = size >> CACHELINE_MULTIPLE;
                do
                {
                    CLFLUSHOPT(src_alnd, cacheline_iters);
                }while (cacheline_iters--);
                state.ResumeTiming();
                benchmark::DoNotOptimize(func((char*)src_alnd,'X'));
            }
        }
        TearDown();
    Bench_Result(state);
    }

protected:
    char* src;
    char* dst;
    void* src_alnd;
    void* dst_alnd;
};

template <typename MemFunction, typename... Args>
void runStringBenchmark(benchmark::State& state, MemoryMode mode, char alignment, MemFunction func, const char* name, Args... args) {
    StringFixture fixture;
    fixture.strRunBenchmark(state, mode, alignment, func, name, args...);
}

template <typename MemFunction, typename... Args>
void runString_n_Benchmark(benchmark::State& state, MemoryMode mode,char alignment, MemFunction func, const char* name, Args... args) {
    StringFixture fixture;
    fixture.str_n_RunBenchmark(state, mode, alignment, func, name, args...);
}

template <typename MemFunction, typename... Args>
void runstrlenBenchmark(benchmark::State& state, MemoryMode mode, char alignment, MemFunction func, const char* name, Args... args) {
    StringFixture fixture;
    fixture.strlenRunBenchmark(state, mode, alignment, func, name, args...);
}

template <typename MemFunction, typename... Args>
void runsubstrBenchmark(benchmark::State& state, MemoryMode mode, char alignment, MemFunction func, const char* name, Args... args) {
    StringFixture fixture;
    fixture.substrRunBenchmark(state, mode, alignment, func, name, args...);
}

template <typename MemFunction, typename... Args>
void runMiscStringBenchmark(benchmark::State& state, MemoryMode mode, char alignment, MemFunction func, const char* name, Args... args) {
    StringFixture fixture;
    fixture.strchrRunBenchmark(state, mode, alignment, func, name, args...);
}

MemData functionList[] = {
    REGISTER_MEM_FUNCTION(memcpy),
    REGISTER_MEM_FUNCTION(mempcpy),
    REGISTER_MEM_FUNCTION(memmove),
    REGISTER_MEM_FUNCTION(memcmp),
};

Misc_MemData Misc_Mem_functionList[] = {
    REGISTER_MISC_MEM_FUNCTION(memset),
    REGISTER_MISC_MEM_FUNCTION(memchr),
};

Misc_StrData MiscStrfunctionList[] = {
    {"strchr", (const char *(*)(char *, int)) my_strchr},
};

StrData strfunctionList[] = {
    REGISTER_STR_FUNCTION(strcpy),
    REGISTER_STR_FUNCTION(strcmp),
    REGISTER_STR_FUNCTION(strcat),
    REGISTER_STR_FUNCTION(strspn),
};

substr subfunctionList[] = {
    {"strstr", (char *(*)(char *, const char *)) my_strstr},
};

Str_n_Data str_n_functionList[] = {
    REGISTER_STR_N_FUNCTION(strncpy),
    REGISTER_STR_N_FUNCTION(strncmp),
    REGISTER_STR_N_FUNCTION(strncat),
};

Strlen_Data Strlen_functionList[] = {
    {"strlen",  (void *(*)(const char*)) strlen},
};

enum class Parameters
{
    function_name = 1,  // argv[1]
    function_mode,      // argv[2]
    start,              // argv[3]
    end,                // argv[4]
    iterations,         // argv[5]
    align               // argv[6]
};

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    srand(time(0));
    char mode='c', alignment = 'd';
    unsigned int size_start, size_end, iter = 0;
    std::string func;

    func = argv[static_cast<int>(Parameters::function_name)];

    if(argv[static_cast<int>(Parameters::function_mode)]!=NULL)
        mode = *argv[static_cast<int>(Parameters::function_mode)];

    if (argv[static_cast<int>(Parameters::start)] != NULL)
        size_start = std::atoi(argv[static_cast<int>(Parameters::start)]);
    if (argv[static_cast<int>(Parameters::end)] != NULL)
        size_end = std::atoi(argv[static_cast<int>(Parameters::end)]);

    if (argv[static_cast<int>(Parameters::iterations)] != NULL)
        iter = std::atoi(argv[static_cast<int>(Parameters::iterations)]);

    if(argv[static_cast<int>(Parameters::align)]!=NULL)
        alignment= *argv[static_cast<int>(Parameters::align)];

    std::cout<<"FUNCTION: "<<func<<" MODE: "<<mode<<std::endl;
    std::cout<<"SIZE: "<<size_start<<" "<<size_end<<std::endl;

    MemoryMode operation;
    if( mode == 'u')
        operation = UNCACHED;
    else
        operation = CACHED;

    auto memoryBenchmark = [&](benchmark::State& state) {
        for (const auto &MemData : functionList) {
            if (func == MemData.functionName) {
                runMemoryBenchmark(state, operation, alignment, MemData.functionPtr, MemData.functionName);
            }
        }
    };

    auto Misc_memoryBenchmark = [&](benchmark::State& state) {
        for (const auto &Misc_MemData : Misc_Mem_functionList) {
            if (func == Misc_MemData.functionName) {
                runMiscMemoryBenchmark(state, operation, alignment, Misc_MemData.functionPtr, Misc_MemData.functionName);
            }
        }
    };

    auto stringBenchmark = [&](benchmark::State& state) {
        for (const auto &strData : strfunctionList) {
            if (func == strData.functionName) {
                runStringBenchmark(state, operation, alignment, strData.functionPtr, strData.functionName);
            }
        }
    };

    auto string_n_Benchmark = [&](benchmark::State& state) {
        for (const auto &strData : str_n_functionList) {
            if (func == strData.functionName) {
                runString_n_Benchmark(state, operation, alignment, strData.functionPtr, strData.functionName);
            }
        }
    };

    auto misc_string_Benchmark = [&](benchmark::State& state) {
        for (const auto &strData : MiscStrfunctionList) {
            if (func == strData.functionName) {
                runMiscStringBenchmark(state, operation, alignment, strData.functionPtr, strData.functionName);
            }
        }
    };

    auto strlenBenchmark = [&](benchmark::State& state) {
        for (const auto &strData : Strlen_functionList) {
            if (func == strData.functionName) {
                runstrlenBenchmark(state, operation, alignment, strData.functionPtr, strData.functionName);
            }
        }
    };

    auto substrBenchmark = [&](benchmark::State& state) {
        for (const auto &strData : subfunctionList) {
            if (func == strData.functionName) {
                runsubstrBenchmark(state, operation, alignment, strData.functionPtr, strData.functionName);
            }
        }
    };
    //Register Benchmark
    std::string _Mode;
    if(operation == UNCACHED)
        _Mode="_UNCACHED";
    else
        _Mode="_CACHED";

    if(iter == 0)
    {

        if (func.compare(0, 3, "mem") == 0)
        {
            if(func.compare(0, 4, "mems")== 0 || func.compare(0, 5, "memch")== 0)
                benchmark::RegisterBenchmark((func + _Mode).c_str(), Misc_memoryBenchmark)->RangeMultiplier(2)->Range(size_start, size_end);
            else
                benchmark::RegisterBenchmark((func + _Mode).c_str(), memoryBenchmark)->RangeMultiplier(2)->Range(size_start, size_end);
        }
        else if(func.compare(0, 3, "str") == 0)
        {
            if(func.compare(0, 4, "strn") == 0 )
                benchmark::RegisterBenchmark((func + _Mode).c_str(), string_n_Benchmark)->RangeMultiplier(2)->Range(size_start, size_end);
            else if(func.compare(0, 4, "strl") == 0)
                benchmark::RegisterBenchmark((func + _Mode).c_str(), strlenBenchmark)->RangeMultiplier(2)->Range(size_start, size_end);
            else if(func.compare(0, 6, "strstr") == 0)
                benchmark::RegisterBenchmark((func + _Mode).c_str(), substrBenchmark)->RangeMultiplier(2)->Range(size_start, size_end);
            else if(func.compare(0, 6, "strchr") == 0)
                benchmark::RegisterBenchmark((func + _Mode).c_str(), misc_string_Benchmark)->RangeMultiplier(2)->Range(size_start, size_end);
            else
                benchmark::RegisterBenchmark((func + _Mode).c_str(), stringBenchmark)->RangeMultiplier(2)->Range(size_start, size_end);
        }
    }
    //Iterative Benchmark
    else
    {
        if (func.compare(0, 3, "mem") == 0)
        {
            if(func.compare(0, 4, "mems")== 0 || func.compare(0, 5, "memch")== 0)
                benchmark::RegisterBenchmark((func + _Mode).c_str(), Misc_memoryBenchmark)->RangeMultiplier(2)->DenseRange(size_start, size_end, iter);
            else
                benchmark::RegisterBenchmark((func + _Mode).c_str(), memoryBenchmark)->RangeMultiplier(2)->DenseRange(size_start, size_end, iter);

        }
        else if(func.compare(0, 3, "str") == 0)
        {
            if(func.compare(0, 4, "strn") == 0 )
                benchmark::RegisterBenchmark((func + _Mode).c_str(), string_n_Benchmark)->RangeMultiplier(2)->DenseRange(size_start, size_end, iter);
            else if(func.compare(0, 4, "strl") == 0)
                benchmark::RegisterBenchmark((func + _Mode).c_str(), strlenBenchmark)->RangeMultiplier(2)->DenseRange(size_start, size_end, iter);
            else if(func.compare(0, 6, "strstr") == 0)
                benchmark::RegisterBenchmark((func + _Mode).c_str(), substrBenchmark)->RangeMultiplier(2)->DenseRange(size_start, size_end, iter);
            else if(func.compare(0, 6, "strchr") == 0)
                benchmark::RegisterBenchmark((func + _Mode).c_str(), misc_string_Benchmark)->RangeMultiplier(2)->DenseRange(size_start, size_end, iter);
            else
                benchmark::RegisterBenchmark((func + _Mode).c_str(), stringBenchmark)->RangeMultiplier(2)->DenseRange(size_start, size_end, iter);

        }
    }

    ::benchmark::RunSpecifiedBenchmarks();
}
