// #include <benchmark/benchmark.h>

// void Ddg_s()
// {
//   float _vdata[5000];
//   float _vmmu[5000];
//   float _vddg[5000];
//   uint32_t _range = 4500;
//   uint32_t _w = 100;
//   _vddg[1] = static_cast<float>(2.0);
//   for(uint32_t i = 0; i < 5000; i++) {
//     _vddg[i] = ((_vdata[i + _w] - _vmmu[i + 1]) + (_vdata[i] - _vmmu[i]));
//   }

//   _vddg[_range] = static_cast<float>(0.0);
// }

// static void BM_SomeFunction(benchmark::State& state) {
//   // Perform setup here
//   for (auto _ : state) {
//     // This code gets timed
//     Ddg_s();
//   }
// }
// // Register the function as a benchmark
// BENCHMARK(BM_SomeFunction);

// BENCHMARK_MAIN();


int square(int num) {
    return num * num;
}

int main() {
    int i = 0;
    return square(i);
}
