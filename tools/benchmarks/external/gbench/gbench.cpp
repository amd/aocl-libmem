#include <benchmark/benchmark.h>
#include <cstdlib>
#include <cstring>
#include <iostream>

static void cached_memcpy(benchmark::State& state) {
  char* src = new char[state.range(0)];
  char* dst = new char[state.range(0)];

  for (auto _ : state) {
    benchmark::DoNotOptimize(memcpy(dst, src, state.range(0)));
  }
  state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
  state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);
  delete[] src;
  delete[] dst;
}

static void cached_memset(benchmark::State& state) {
  char* src = new char[state.range(0)];

  for (auto _ : state) {
    benchmark::DoNotOptimize(memset(src,'x', state.range(0)));
  }

  state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
  state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);
  delete[] src;
}

static void cached_memmove(benchmark::State& state) {
  char* src = new char[state.range(0)];
  char* dst = new char[state.range(0)];

  for (auto _ : state) {
    benchmark::DoNotOptimize(memmove(dst, src, state.range(0)));
  }

  state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
  state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);
  delete[] src;
  delete[] dst;
}

static void cached_memcmp(benchmark::State& state) {
  char* src = new char[state.range(0)];
  char* dst = new char[state.range(0)];

  memset(src, 'x', state.range(0));
  memset(dst, 'x', state.range(0));

  *(src + state.range(0) - 1) ='$';

  for (auto _ : state) {
    benchmark::DoNotOptimize(memcmp(dst, src, state.range(0)));
  }

  state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
  state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);
  delete[] src;
  delete[] dst;
}

static void cached_strcpy(benchmark::State& state) {
  char* src = new char[state.range(0)];
  char* dst = new char[state.range(0)];
  for(unsigned long i=0;i<state.range(0);i++) {
    *(src + i) = 'a' +rand()%26;
  }
  *(src + state.range(0) -1) ='\0';
  for (auto _ : state) {
    benchmark::DoNotOptimize(strcpy(dst, src));
  }
  state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
  state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);
  delete[] src;
  delete[] dst;
}

static void cached_strncpy(benchmark::State& state) {
  char* src = new char[state.range(0)];
  char* dst = new char[state.range(0)];
  for(unsigned long i=0;i<state.range(0);i++) {
    *(src + i) = 'a' +rand()%26;
  }
  *(src + state.range(0) -1) ='\0';
  for (auto _ : state) {
    benchmark::DoNotOptimize(strncpy(dst, src, state.range(0)));
  }
  state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
  state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);
  delete[] src;
  delete[] dst;
}

static void uncached_memcpy(benchmark::State& state) {
const size_t bufferSize = state.range(0) * 4096 ;
char *src = new char[bufferSize];
char *dst = new char[bufferSize];
srand(time(0));
  for (auto _ : state) {
    benchmark::DoNotOptimize(memcpy(src + (rand()%1024)  , dst + (rand()%1024) ,state.range(0)));
   }

  state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
  state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);
  delete[] src;
  delete[] dst;
}

static void uncached_memset(benchmark::State& state) {
const size_t bufferSize = state.range(0) * 4096 ;
char *src = new char[bufferSize];
srand(time(0));
  for (auto _ : state) {
    benchmark::DoNotOptimize(memset(src + (rand()%1024),'x', state.range(0)));
    }
    delete[] src;
  state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
  state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);

}

static void uncached_memmove(benchmark::State& state) {
  const size_t bufferSize = state.range(0) * 4096 ;
char *src = new char[bufferSize];
char *dst = new char[bufferSize];
srand(time(0));
  for (auto _ : state) {
    benchmark::DoNotOptimize(memmove(src + (rand()%1024)  , dst + (rand()%1024) ,state.range(0)));
   }

  state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
  state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);
  delete[] src;
  delete[] dst;
}

static void uncached_memcmp(benchmark::State& state) {

  for (auto _ : state) { 
    state.PauseTiming();
    char* src = new char[state.range(0)];
    char* dst = new char[state.range(0)];

  for(unsigned long i=0;i<state.range(0);i++) {
    *(src + i) = *(dst + i) ='a' +rand()%26;
  }
  *(src + state.range(0) -1) ='$';
   state.ResumeTiming();
   benchmark::DoNotOptimize(memcmp(dst, src, state.range(0)));

   state.PauseTiming();
   delete[] src;
   delete[] dst;
   state.ResumeTiming();
  }
  state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
  state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);
}

static void uncached_strcpy(benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    char *src = new char[state.range(0)];
    char *dst = new char[state.range(0)];

    for(unsigned long i=0;i<state.range(0);i++) {
    *(src + i) = 'a' +rand()%26;
    }
    *(src + state.range(0) -1) ='\0';

    state.ResumeTiming();
    benchmark::DoNotOptimize(strcpy(dst, src));

    state.PauseTiming();
    delete[] src;
    delete[] dst;
    state.ResumeTiming();
  }
  state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
  state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);

}

static void uncached_strncpy(benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    char *src = new char[state.range(0)];
    char *dst = new char[state.range(0)];

    for(unsigned long i=0;i<state.range(0);i++) {
    *(src + i) = 'a' +rand()%26;
    }
    *(src + state.range(0) -1) ='\0';

    state.ResumeTiming();
    benchmark::DoNotOptimize(strncpy(dst, src, state.range(0)));

    state.PauseTiming();
    delete[] src;
    delete[] dst;
    state.ResumeTiming();
  }
  state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
  state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);

}


typedef struct
{
    const char *func_name;
    void  (*func)(benchmark::State&);
} libmem_func;

libmem_func supp_funcs[]=
{
    {"cmemcpy",  cached_memcpy},
    {"cmemset",  cached_memset},
    {"cmemmove", cached_memmove},
    {"cmemcmp",  cached_memcmp},
    {"cstrcpy",  cached_strcpy},
    {"cstrncpy", cached_strncpy},
    {"umemcpy",  uncached_memcpy},
    {"umemset",  uncached_memset},
    {"umemmove", uncached_memmove},
    {"umemcmp",  uncached_memcmp},
    {"ustrcpy",  uncached_strcpy},
    {"ustrncpy", uncached_strncpy},
    {"none",    NULL}
};

int AddBenchmarks(std::string func, char mode,unsigned  int start, unsigned int end, unsigned int iter) {

libmem_func *lm_func = &supp_funcs[0]; //default func is cached-memcpy

// {c,u} + Mem_function
std::string bench_function = mode + func;
const char* mode_function = bench_function.c_str();

for (int idx = 0; idx <= sizeof(supp_funcs)/sizeof(supp_funcs[0]); idx++)
{
    if (!strcmp(supp_funcs[idx].func_name, mode_function))
    {
        lm_func = &supp_funcs[idx];
        break;
    }
}

    if(iter)
    BENCHMARK(lm_func->func)->DenseRange(start,end,iter);

    else
    BENCHMARK(lm_func->func)->RangeMultiplier(2)->Range(start,end);

  return 0;
}

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
  AddBenchmarks(func, mode, size_start, size_end, iter);
  ::benchmark::Initialize(&argc, argv);
  ::benchmark::RunSpecifiedBenchmarks();
  std::cout<<std::endl;
  return 0;
}
