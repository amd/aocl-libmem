#include <benchmark/benchmark.h>
#include <cstring>
#include <iostream>
#include <string>

enum MemoryMode {
    CACHED,
    UNCACHED
};

void Bench_Result(benchmark::State& state)
{
state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);
}

class MemoryCopyFixture {
public:
    void SetUp(size_t size) {
        src = new char[size];
        dst = new char[size];
        memcpy(dst,src,size);
    }

    void TearDown() {
        delete[] src;
        delete[] dst;
    }

    template <typename MemFunction, typename... Args>
    void memRunBenchmark(benchmark::State& state, MemoryMode mode, MemFunction func, const char* name, Args... args) {
        int size = state.range(0);

        if(mode == CACHED)
        {
            SetUp(size);
            for (auto _ : state) {
            benchmark::DoNotOptimize(func(dst, src, size));
            }
        }
        else
        {
            SetUp(state.range(0) * 4096);
            srand(time(0));
            for (auto _ : state) {
            benchmark::DoNotOptimize(func(src + (rand()%1024)  , dst + (rand()%1024) ,state.range(0)));
            }
        }

        Bench_Result(state);
        TearDown();
    }

    template <typename MemFunction, typename... Args>
    void memsetRunBenchmark(benchmark::State& state, MemoryMode mode, MemFunction func, const char* name, Args... args) {
        int size = state.range(0);
        if(mode == CACHED)
        {
            SetUp(size);
            for (auto _ : state) {
            benchmark::DoNotOptimize(func(src, 'x', size));
            }
        }
        else
        {
            SetUp(state.range(0) * 4096);
            srand(time(0));
            for (auto _ : state) {
            benchmark::DoNotOptimize(func(src + (rand()%1024), 'x',state.range(0)));
            }
        }

        Bench_Result(state);
        TearDown();
    }

protected:
    char* src;
    char* dst;
};

template <typename MemFunction, typename... Args>
void runMemoryBenchmark(benchmark::State& state,  MemoryMode mode, MemFunction func, const char* name, Args... args) {
    MemoryCopyFixture fixture;
    fixture.memRunBenchmark(state, mode, func, name, args...);
}
template <typename MemFunction, typename... Args>
void runMemsetMemoryBenchmark(benchmark::State& state,  MemoryMode mode, MemFunction func, const char* name, Args... args) {
    MemoryCopyFixture fixture;
    fixture.memsetRunBenchmark(state, mode, func, name, args...);
}


class StringFixture {
public:
    void SetUp(size_t size) {
        src = new char[size];
        memset(src,'x', size);
        src[size -1 ] = '\0';
        dst = new char[size];
        strcpy(dst,src);
    }

    void TearDown() {
        delete[] src;
        delete[] dst;
    }

    template <typename MemFunction, typename... Args>
    void strRunBenchmark(benchmark::State& state, MemoryMode mode, MemFunction func, const char* name, Args... args) {
        int size = state.range(0);
        if(mode == CACHED)
        {
            SetUp(size);
            for (auto _ : state) {
            benchmark::DoNotOptimize(func(dst, src));
            }
            TearDown();
        }
        else
        {
            for (auto _ : state) {
            state.PauseTiming();
            SetUp(size);
            state.ResumeTiming();
            benchmark::DoNotOptimize(func(dst, src));
            state.PauseTiming();
            TearDown();
            state.ResumeTiming();
            }
        }
    Bench_Result(state);
    }

    template <typename MemFunction, typename... Args>
    void strlenRunBenchmark(benchmark::State& state, MemoryMode mode, MemFunction func, const char* name, Args... args) {
        int size = state.range(0);
        if(mode == CACHED)
        {
            SetUp(size);
            for (auto _ : state) {
            benchmark::DoNotOptimize(func(src));
            }
            TearDown();
        }
        else
        {
            for (auto _ : state) {
            state.PauseTiming();
            SetUp(size);
            state.ResumeTiming();
            benchmark::DoNotOptimize(func(src));
            state.PauseTiming();
            TearDown();
            state.ResumeTiming();
            }
        }
    Bench_Result(state);
    }

    template <typename MemFunction, typename... Args>
    void str_n_RunBenchmark(benchmark::State& state, MemoryMode mode, MemFunction func, const char* name, Args... args) {
        int size = state.range(0);
        if(mode == CACHED)
        {
            SetUp(size);
            for (auto _ : state) {
            benchmark::DoNotOptimize(func(dst, src, size));
            }
            TearDown();
        }
        else
        {
            for (auto _ : state) {
            state.PauseTiming();
            SetUp(size);
            state.ResumeTiming();
            benchmark::DoNotOptimize(func(dst, src, size));
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
};

template <typename MemFunction, typename... Args>
void runStringBenchmark(benchmark::State& state, MemoryMode mode, MemFunction func, const char* name, Args... args) {
    StringFixture fixture;
    fixture.strRunBenchmark(state, mode, func, name, args...);
}

template <typename MemFunction, typename... Args>
void runString_n_Benchmark(benchmark::State& state, MemoryMode mode, MemFunction func, const char* name, Args... args) {
    StringFixture fixture;
    fixture.str_n_RunBenchmark(state, mode, func, name, args...);
}

template <typename MemFunction, typename... Args>
void runstrlenBenchmark(benchmark::State& state, MemoryMode mode, MemFunction func, const char* name, Args... args) {
    StringFixture fixture;
    fixture.strlenRunBenchmark(state, mode, func, name, args...);
}


struct MemData {
    const char *functionName;
    void *(*functionPtr)(void *dest, const void *src, size_t count);
};

MemData functionList[] = {
    {"memcpy", (void *(*)(void *, const void *, size_t))memcpy},
    {"mempcpy", (void *(*)(void *, const void *, size_t))mempcpy},
    {"memmove", (void *(*)(void *, const void *, size_t))memmove},
    {"memcmp", (void *(*)(void *, const void *, size_t))memcmp},
};

//Functions with diff signatrues
struct Memset_Data {
    const char *functionName;
    void *(*functionPtr)(void*src, int value, size_t size);
};

Memset_Data Memset_functionList[] = {
    {"memset",  (void *(*)(void *, int, size_t))memset},
};

struct StrData {
    const char *functionName;
    void *(*functionPtr)(char *dest, const char *src);
};

StrData strfunctionList[] = {
    {"strcpy", (void *(*)(char *, const char *))strcpy},
    {"strcmp", (void *(*)(char *, const char *))strcmp},
};

struct Str_n_Data {
    const char *functionName;
    void *(*functionPtr)(char *dest, const char *src, size_t n);
};

Str_n_Data str_n_functionList[] = {
    {"strncpy", (void *(*)(char *, const char *, size_t))strncpy},
    {"strncmp", (void *(*)(char *, const char *, size_t))strncmp},
};

struct Strlen_Data {
    const char *functionName;
    void *(*functionPtr)(const char *src);
};

Strlen_Data Strlen_functionList[] = {
    {"strlen",  (void *(*)(const char*)) strlen},
};

int main(int argc, char** argv) {
    char mode='c';
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
        runMemoryBenchmark(state, operation, MemData.functionPtr, MemData.functionName);
        }
    }
    };

    auto memsetBenchmark = [&](benchmark::State& state) {
        for (const auto &Memset_Data : Memset_functionList) {
    if (func == Memset_Data.functionName) {
        runMemsetMemoryBenchmark(state, operation, Memset_Data.functionPtr, Memset_Data.functionName);
        }
    }
    };

    auto stringBenchmark = [&](benchmark::State& state) {
        for (const auto &strData : strfunctionList) {
    if (func == strData.functionName) {
        runStringBenchmark(state, operation, strData.functionPtr, strData.functionName);
        }
    }
    };

    auto string_n_Benchmark = [&](benchmark::State& state) {
        for (const auto &strData : str_n_functionList) {
    if (func == strData.functionName) {
        runString_n_Benchmark(state, operation, strData.functionPtr, strData.functionName);
        }
    }
    };

    auto strlenBenchmark = [&](benchmark::State& state) {
        for (const auto &strData : Strlen_functionList) {
    if (func == strData.functionName) {
        runstrlenBenchmark(state, operation, strData.functionPtr, strData.functionName);
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
            if(func.compare(0, 4, "mems")== 0)
                benchmark::RegisterBenchmark((func + _Mode).c_str(), memsetBenchmark)->RangeMultiplier(2)->Range(size_start, size_end);
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
            if(func.compare(0, 4, "mems") == 0)
                benchmark::RegisterBenchmark((func + _Mode).c_str(), memsetBenchmark)->RangeMultiplier(2)->DenseRange(size_start, size_end, iter);
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
