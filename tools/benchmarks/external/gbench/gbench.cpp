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


#if defined(__AVX512F__)
    #define VEC_SIZE 64
#else
    #define VEC_SIZE 32
#endif

#define CL_SIZE 64


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

        if(mode == CACHED)
        {
            SetUp(size, alignment);
            for (auto _ : state) {
            benchmark::DoNotOptimize(func(dst_alnd, src_alnd, size));
            }
        }
        else
        {
            SetUp(state.range(0) * 4096, alignment);
            srand(time(0));
            for (auto _ : state) {
            benchmark::DoNotOptimize(func(src_alnd + (rand()%1024), dst_alnd + (rand()%1024) ,state.range(0)));
            }
        }

        Bench_Result(state);
        TearDown();
    }

    template <typename MemFunction, typename... Args>
    void Misc_memRunBenchmark(benchmark::State& state, MemoryMode mode, char alignment, MemFunction func, const char* name, Args... args) {
        int size = state.range(0);
        if(mode == CACHED)
        {
            SetUp(size, alignment);
            for (auto _ : state) {
            benchmark::DoNotOptimize(func(src_alnd, 'x', size));
            }
        }
        else
        {
            SetUp(state.range(0) * 4096, alignment);
            srand(time(0));
            for (auto _ : state) {
            benchmark::DoNotOptimize(func(src_alnd + (rand()%1024), 'x',state.range(0)));
            }
        }

        Bench_Result(state);
        TearDown();
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

        char* val = static_cast<char*>(src_alnd);
        val[size -1] = '\0';
        strcpy((char*)dst_alnd,(char*)src_alnd);
    }

    void TearDown() {
        delete[] src;
        delete[] dst;
    }

    template <typename MemFunction, typename... Args>
    void strRunBenchmark(benchmark::State& state, MemoryMode mode, char alignment, MemFunction func, const char* name, Args... args) {
        int size = state.range(0);
        if(mode == CACHED)
        {
            if (name == "strcat")
            {
                SetUp(size, alignment);           
                for (auto _ : state) {
                    char *val = static_cast<char*>(dst_alnd);
                    val[size -1] = '\0';
                    benchmark::DoNotOptimize(func((char*)dst_alnd, (char*)src_alnd));
                }
            }
            else
            {
                SetUp(size, alignment);
                for (auto _ : state) {
                    benchmark::DoNotOptimize(func((char*)dst_alnd, (char*)src_alnd));
                }
            }

            TearDown();
        }
        else
        {
            if (name == "strcat")
            {   for (auto _ : state) {
                state.PauseTiming();
                SetUp(size, alignment);                       
                char* val = static_cast<char*>(src_alnd);
                val[size/2] = '\0';
                strcpy((char*)dst_alnd,(char*)src_alnd);
                state.ResumeTiming();
                benchmark::DoNotOptimize(func((char*)dst_alnd, (char*)src_alnd));
                state.PauseTiming();
                TearDown();
                state.ResumeTiming();
                }
            }
            else
            {
                for (auto _ : state) {
                state.PauseTiming();
                SetUp(size, alignment);
                state.ResumeTiming();
                benchmark::DoNotOptimize(func((char*)dst_alnd, (char*)src_alnd));
                state.PauseTiming();
                TearDown();
                state.ResumeTiming();
            }
            }
        }
    Bench_Result(state);
    }

    template <typename MemFunction, typename... Args>
    void strlenRunBenchmark(benchmark::State& state, MemoryMode mode,char alignment, MemFunction func, const char* name, Args... args) {
        int size = state.range(0);
        if(mode == CACHED)
        {
            SetUp(size, alignment);
            for (auto _ : state) {
            benchmark::DoNotOptimize(strlen((char*)src_alnd));
            }
            TearDown();
        }
        else
        {
            for (auto _ : state) {
            state.PauseTiming();
            SetUp(size, alignment);
            state.ResumeTiming();
            benchmark::DoNotOptimize(func((char*)src_alnd));
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
        if(mode == CACHED)
        {
            SetUp(size, alignment);
            for (auto _ : state) {
            benchmark::DoNotOptimize(func((char*)dst_alnd, (char*)src_alnd, size));
            }
            TearDown();
        }
        else
        {
            for (auto _ : state) {
            state.PauseTiming();
            SetUp(size, alignment);
            state.ResumeTiming();
            benchmark::DoNotOptimize(func((char*)dst_alnd, (char*)src_alnd, size));
            state.PauseTiming();
            TearDown();
            state.ResumeTiming();
            }
        }
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


struct MemData {
    const char *functionName;
    void *(*functionPtr)(void *dest, const void *src, size_t count);
};

MemData functionList[] = {
    REGISTER_MEM_FUNCTION(memcpy),
    REGISTER_MEM_FUNCTION(mempcpy),
    REGISTER_MEM_FUNCTION(memmove),
    REGISTER_MEM_FUNCTION(memcmp),
};

//Functions with diff signatrues
struct Misc_MemData {
    const char *functionName;
    void *(*functionPtr)(void*src, int value, size_t size);
};

Misc_MemData Misc_Mem_functionList[] = {
    REGISTER_MISC_MEM_FUNCTION(memset),
    REGISTER_MISC_MEM_FUNCTION(memchr),
};

struct StrData {
    const char *functionName;
    void *(*functionPtr)(char *dest, const char *src);
};

StrData strfunctionList[] = {
    REGISTER_STR_FUNCTION(strcpy),
    REGISTER_STR_FUNCTION(strcmp),
    REGISTER_STR_FUNCTION(strcat),
};

struct Str_n_Data {
    const char *functionName;
    void *(*functionPtr)(char *dest, const char *src, size_t n);
};

#define REGISTER_STR_N_FUNCTION(name) \
    { #name, (void *(*)(char *,const char *, size_t))name }

Str_n_Data str_n_functionList[] = {
    REGISTER_STR_N_FUNCTION(strncpy),
    REGISTER_STR_N_FUNCTION(strncmp),
};

struct Strlen_Data {
    const char *functionName;
    void *(*functionPtr)(const char *src);
};

Strlen_Data Strlen_functionList[] = {
    {"strlen",  (void *(*)(const char*)) strlen},
};

int main(int argc, char** argv) {
    char mode='c', alignment = 'd';
    unsigned int size_start, size_end, iter = 0;
    std::string func;

    func = argv[2];

    if(argv[3]!=NULL)
        mode= *argv[3];

    if (argv[4] != NULL)
        size_start = atoi(argv[4]);
    if (argv[5] != NULL)
        size_end = atoi(argv[5]);

    if (argv[6] != NULL)
        iter = atoi(argv[6]);

    if(argv[7]!=NULL)
        alignment= *argv[7];

    std::cout<<"FUNCTION: "<<func<<" MODE: "<<mode<<std::endl;
    std::cout<<"SIZE: "<<size_start<<" "<<size_end<<std::endl;

    MemoryMode operation;
    if( mode == 'u')
        operation = UNCACHED;
    else
        operation = CACHED;

    ::benchmark::Initialize(&argc, argv);

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

    auto strlenBenchmark = [&](benchmark::State& state) {
        for (const auto &strData : Strlen_functionList) {
    if (func == strData.functionName) {
        runstrlenBenchmark(state, operation, alignment, strData.functionPtr, strData.functionName);
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
            else
                benchmark::RegisterBenchmark((func + _Mode).c_str(), stringBenchmark)->RangeMultiplier(2)->DenseRange(size_start, size_end, iter);

        }
    }

    ::benchmark::RunSpecifiedBenchmarks();
}
