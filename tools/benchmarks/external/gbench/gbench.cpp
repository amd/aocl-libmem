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

static void uncached_memcpy(benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    char *src = new char[state.range(0)];
    char *dst = new char[state.range(0)];
    state.ResumeTiming();
    benchmark::DoNotOptimize(memcpy(dst, src, state.range(0)));

    state.PauseTiming();
    delete[] src;
    delete[] dst;
    state.ResumeTiming();
  }
  state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
  state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);

}

static void uncached_memset(benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    char *src = new char[state.range(0)];
    state.ResumeTiming();
    benchmark::DoNotOptimize(memset(src,'x', state.range(0)));

    state.PauseTiming();
    delete[] src;
    state.ResumeTiming();
  }
  state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
  state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);

}

static void uncached_memmove(benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    char *src = new char[state.range(0)];
    char *dst = new char[state.range(0)];
    state.ResumeTiming();
    benchmark::DoNotOptimize(memmove(dst, src, state.range(0)));

    state.PauseTiming();
    delete[] src;
    delete[] dst;
    state.ResumeTiming();
  }
  state.counters["Throughput(Bytes/s)"]=benchmark::Counter(state.iterations()*state.range(0),benchmark::Counter::kIsRate);
  state.counters["Size(Bytes)"]=benchmark::Counter(static_cast<double>(state.range(0)),benchmark::Counter::kDefaults,benchmark::Counter::kIs1024);

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

int AddBenchmarks(std::string func, char mode,unsigned  int start, unsigned int end, unsigned int iter) {
  if (iter)
  {
    if(func=="memcpy")
    {
      if(mode == 'c')
        BENCHMARK(cached_memcpy)->DenseRange(start,end,iter);
      else if(mode =='u')
        BENCHMARK(uncached_memcpy)->DenseRange(start,end,iter);
    }
    else if(func=="memset")
    {
      if(mode =='c')
        BENCHMARK(cached_memset)->DenseRange(start,end,iter);
      else if(mode =='u')
        BENCHMARK(uncached_memset)->DenseRange(start,end,iter);
    }
    else if(func=="memmove")
    {
      if(mode =='c')
        BENCHMARK(cached_memmove)->DenseRange(start,end,iter);
      else if(mode =='u')
        BENCHMARK(uncached_memmove)->DenseRange(start,end,iter);
    }
    else if(func=="memcmp")
    {
      if(mode =='c')
        BENCHMARK(cached_memcmp)->DenseRange(start,end,iter);
      else if(mode =='u')
        BENCHMARK(uncached_memcmp)->DenseRange(start,end,iter);
    }
    else if(func=="strcpy")
    {
      if(mode =='c')
        BENCHMARK(cached_strcpy)->DenseRange(start,end,iter);
      else if(mode =='u')
        BENCHMARK(uncached_strcpy)->DenseRange(start,end,iter);
    }
  }
  else
  {
    if(func=="memcpy")
    {
      if(mode == 'c')
        BENCHMARK(cached_memcpy)->RangeMultiplier(2)->Range(start,end);
      else if(mode =='u')
        BENCHMARK(uncached_memcpy)->RangeMultiplier(2)->Range(start,end);
    }
    else if(func=="memset")
    {
      if(mode =='c')
        BENCHMARK(cached_memset)->RangeMultiplier(2)->Range(start,end);
      else if(mode =='u')
        BENCHMARK(uncached_memset)->RangeMultiplier(2)->Range(start,end);
    }
    else if(func=="memmove")
    {
      if(mode =='c')
        BENCHMARK(cached_memmove)->RangeMultiplier(2)->Range(start,end);
      else if(mode =='u')
        BENCHMARK(uncached_memmove)->RangeMultiplier(2)->Range(start,end);
    }
    else if(func=="memcmp")
    {
      if(mode =='c')
        BENCHMARK(cached_memcmp)->RangeMultiplier(2)->Range(start,end);
      else if(mode =='u')
        BENCHMARK(uncached_memcmp)->RangeMultiplier(2)->Range(start,end);
    }
    else if(func=="strcpy")
    {
      if(mode =='c')
        BENCHMARK(cached_strcpy)->RangeMultiplier(2)->Range(start,end);
      else if(mode =='u')
        BENCHMARK(uncached_strcpy)->RangeMultiplier(2)->Range(start,end);
    }
  }
  return 0;
}

int main(int argc, char** argv) {
  char mode='c';
  unsigned int size_start, size_end, iter = 0;
  std::string func;

  func = argv[2];

  std::cout<<func<<"FUNCTION";

  if(argv[3]!=NULL)
    mode= *argv[3];

  if (argv[4] != NULL)
    size_start = atoi(argv[4]);
  if (argv[5] != NULL)
    size_end = atoi(argv[5]);

  if (argv[6] != NULL)
    iter = atoi(argv[6]);

  std::cout<<"SIZE"<<size_start<<" "<<size_end<<std::endl;
  AddBenchmarks(func, mode, size_start, size_end, iter);
  ::benchmark::Initialize(&argc, argv);
  ::benchmark::RunSpecifiedBenchmarks();
  std::cout<<std::endl;
  return 0;
}
