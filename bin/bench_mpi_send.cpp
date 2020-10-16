#include "../include/cuda_runtime.hpp"
#include "../include/env.hpp"
#include "../include/logging.hpp"
#include "statistics.hpp"

#include <mpi.h>
#include <nvToolsExt.h>

#include <chrono>
#include <sstream>

typedef std::chrono::system_clock Clock;
typedef std::chrono::duration<double> Duration;
typedef std::chrono::time_point<Clock, Duration> Time;

struct BenchResult {
  double pingPongTime;
};

BenchResult bench(size_t numBytes,
                  bool tempi, // use tempi or not
                  const int nIters) {

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  // configure TEMPI
  environment::noTempi = !tempi;

  // create device allocations
  void *src = {}, *dst = {};
  CUDA_RUNTIME(cudaSetDevice(0));
  CUDA_RUNTIME(cudaMalloc(&src, numBytes));
  CUDA_RUNTIME(cudaMalloc(&dst, numBytes));

  Statistics stats;
  nvtxRangePush("loop");
  for (int n = 0; n < nIters; ++n) {
    int position = 0;
    auto start = Clock::now();
    if (0 == rank) {
      nvtxRangePush("send_0");
      MPI_Send(src, numBytes, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
      nvtxRangePop();
      MPI_Recv(dst, numBytes, MPI_BYTE, 1, 0, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);

    } else {
      nvtxRangePush("recv_1");
      MPI_Recv(dst, numBytes, MPI_BYTE, 0, 0, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);
      nvtxRangePop();
      MPI_Send(src, numBytes, MPI_BYTE, 0, 0, MPI_COMM_WORLD);
    }
    auto stop = Clock::now();
    Duration dur = stop - start;
    stats.insert(dur.count());
  }
  nvtxRangePop();

  CUDA_RUNTIME(cudaFree(src));
  CUDA_RUNTIME(cudaFree(dst));

  return BenchResult{.pingPongTime = stats.trimean()};
}

int main(int argc, char **argv) {

  environment::noTempi = false;
  MPI_Init(&argc, &argv);

  // run on only ranks 0 and 1
  int size, rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  if (size < 2) {
    if (0 == rank) {
      LOG_FATAL("needs at least 2 ranks");
    } else {
      exit(-1);
    }
  }
  if (!(0 == rank || 1 == rank)) {
    goto finalize;
  }

  { // prevent init bypass
    int nIters = 10;

    BenchResult result;

    std::vector<int> ns = {1,       2,       4,       8,       16,
                           32,      64,      128,     256,     512,
                           1024,    1 << 11, 4096,    1 << 13, 16384,
                           1 << 15, 65536,   1 << 17, 1 << 20, 1 << 30};

    std::vector<bool> tempis = {true, false};

    if (0 == rank) {
      std::cout << "n,tempi, MiB/s\n";
    }

    for (bool tempi : tempis) {
      for (int n : ns) {

        std::string s;
        s = std::to_string(n) + "/" + std::to_string(tempi);

        if (0 == rank) {
          std::cout << s;
          std::cout << "," << n;
          std::cout << "," << tempi;
          std::cout << std::flush;
        }

        nvtxRangePush(s.c_str());
        result = bench(n, tempi, nIters);
        nvtxRangePop();
        if (0 == rank) {
          std::cout << "," << 2 * double(n) / 1024 / 1024 / result.pingPongTime;
          std::cout << std::flush;
        }

        if (0 == rank) {
          std::cout << "\n";
          std::cout << std::flush;
        }
      }
    }
  }
  LOG_DEBUG("at the end");
finalize:
  environment::noTempi = false; // restore this to the same as Init
  MPI_Finalize();
  return 0;
}