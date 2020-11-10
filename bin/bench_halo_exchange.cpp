#include "statistics.hpp"

#include <cuda_runtime.h>
#include <mpi.h>
#include <nvToolsExt.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

// GPU or not
#define USE_CUDA

inline void checkCuda(cudaError_t result, const char *file, const int line) {
  if (result != cudaSuccess) {
    fprintf(stderr, "%s:%d: CUDA Runtime Error %d: %s\n", file, line,
            int(result), cudaGetErrorString(result));
    exit(-1);
  }
}
#define CUDA_RUNTIME(stmt) checkCuda(stmt, __FILE__, __LINE__);

cudaExtent div_cudaExtent(cudaExtent &a, cudaExtent &b) {
  cudaExtent c;
  c.width = a.width / b.width;
  c.height = a.height / b.height;
  c.depth = a.depth / b.depth;
  return c;
}

void to_3d(int *d3, int d1, const int dim[3]) {
  d3[0] = d1 % dim[0];
  d1 /= dim[0];
  d3[1] = d1 % dim[1];
  d1 /= dim[1];
  d3[2] = d1;
  assert(d3[0] < dim[0]);
  assert(d3[1] < dim[1]);
  assert(d3[2] < dim[2]);
}

void to_1d(int *d1, const int d3[3], const int dim[3]) {
  *d1 = d3[0];
  *d1 += d3[1] * dim[0];
  *d1 += d3[2] * dim[1] * dim[0];
  assert(*d1 < dim[0] * dim[1] * dim[2]);
}

/* return the halo size in bytes for a region of size `lcr`, with `radius`, for
 * a send in direction `dir`
 */
size_t halo_size(const int radius,
                 const int lcr[3], // size of the local compute region
                 const int dir[3]) {

  int ext[3]{};
  ext[0] = (0 == dir[0]) ? lcr[0] : radius;
  ext[1] = (0 == dir[1]) ? lcr[1] : radius;
  ext[2] = (0 == dir[2]) ? lcr[2] : radius;
  return ext[0] * ext[1] * ext[2];
}

/* create a derived datatype describing the particular direction
 */
MPI_Datatype halo_type(const int radius, cudaPitchedPtr curr,
                       const int lcr[3], // size of the local compute region
                       const int dir[3], bool exterior) {

  assert(dir[0] >= -1 && dir[0] <= 1);
  assert(dir[1] >= -1 && dir[1] <= 1);
  assert(dir[2] >= -1 && dir[2] <= 1);

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  MPI_Datatype cubetype{};

  // each row either starts at x=0, or radius + xsize
  int pos[3]{};
  if (-1 == dir[0]) { // -x, no offset (exterior), radius (interior)
    pos[0] = (exterior ? 0 : radius);
  } else if (1 == dir[0]) { // +x, xsize (interior), xsize + radius (exterior)
    pos[0] = lcr[0] + (exterior ? radius : 0);
  } else {
    pos[0] = radius;
  }
  if (-1 == dir[1]) {
    pos[1] = (exterior ? 0 : radius);
  } else if (1 == dir[1]) {
    pos[1] = lcr[1] + (exterior ? radius : 0);
  } else {
    pos[1] = radius;
  }
  if (-1 == dir[2]) {
    pos[2] = (exterior ? 0 : radius);
  } else if (1 == dir[2]) {
    pos[2] = lcr[2] + (exterior ? radius : 0);
  } else {
    pos[2] = radius;
  }

  int ext[3]{};
  ext[0] = (0 == dir[0]) ? lcr[0] : radius;
  ext[1] = (0 == dir[1]) ? lcr[1] : radius;
  ext[2] = (0 == dir[2]) ? lcr[2] : radius;

  {
    int ndims = 3;
    // elems in each dimension of the full array
    int array_of_sizes[3]{
        int(curr.pitch), int(curr.ysize),
        pos[2] + ext[2] // array should be at least this big...
    };
    // elems of oldtype in each dimension of the subarray
    int array_of_subsizes[3]{ext[0], ext[1], ext[2]};
    int array_of_starts[3]{pos[0], pos[1],
                           pos[2]}; // starting coordinates of subarray
    int order = MPI_ORDER_C;
    MPI_Datatype oldtype = MPI_BYTE;

    // clang-format off
    std::cerr << "[" << rank << "] "
              << "ndims=" << ndims 
              << " aosz=" 
              << array_of_sizes[0] << "," << array_of_sizes[1] << "," << array_of_sizes[2]
              << " aossz=" 
              << array_of_subsizes[0] << "," << array_of_subsizes[1] << "," << array_of_subsizes[2]
              << " aost=" 
              << array_of_starts[0] << "," << array_of_starts[1] << "," << array_of_starts[2]  
              << "\n";
    // clang-format on

    MPI_Type_create_subarray(ndims, array_of_sizes, array_of_subsizes,
                             array_of_starts, order, oldtype, &cubetype);
  }

  return cubetype;
}

struct BenchResult {
  Statistics pack;
  Statistics alltoallv;
  Statistics unpack;
  Statistics comm;
};

BenchResult bench(MPI_Comm comm, int nquants, int radius, int nIters) {

  BenchResult result;

  int rank, size;
  MPI_Comm_rank(comm, &rank);
  MPI_Comm_size(comm, &size);

  // distributed extent
  cudaExtent distExt = make_cudaExtent(512, 512, 512);
  int dims[3];
  if (1 == size) {
    dims[0] = 1;
    dims[1] = 1;
    dims[2] = 1;
  } else if (2 == size) {
    dims[0] = 2;
    dims[1] = 1;
    dims[2] = 1;
  } else if (4 == size) {
    dims[0] = 2;
    dims[1] = 2;
    dims[2] = 1;
  } else {
    std::cerr << "bad dims\n";
    exit(1);
  }

  // local extent
  cudaExtent locExt;
  locExt.width = distExt.width / dims[0];
  locExt.height = distExt.height / dims[1];
  locExt.depth = distExt.depth / dims[2];
  int lcr[3]{int(locExt.width), int(locExt.height), int(locExt.depth)};

  // allocation extent
  cudaPitchedPtr curr{};
  {
    cudaExtent e = locExt;
    e.width += 2 * radius;
    e.height += 2 * radius;
    e.depth += 2 * radius;

#ifdef USE_CUDA
    CUDA_RUNTIME(cudaMalloc3D(&curr, e));
#else
    // smallest multiple of 512 >= pitch (to match CUDA)
    size_t pitch = (e.width + 511) / 512 * 512;
    curr.pitch = pitch;
    curr.ptr = new char[pitch * e.height * e.depth];
    curr.xsize = e.width;
    curr.ysize = e.height;
#endif
  }
  if (0 == rank) {

    std::cerr << "logical width=" << locExt.width << " pitch=" << curr.pitch
              << "\n";
  }
  MPI_Barrier(MPI_COMM_WORLD);

  /* have each rank take the role of a particular compute region to build the
   * communicator.
   *
   * convert this rank directly into a 3d coordinate
   */
  int mycoord[3];
  to_3d(mycoord, rank, dims);

  // figure out how much I communicate with each neighbor
  std::map<int, int> nbrSendWeight, nbrRecvWeight;
  for (int dz = -1; dz <= 1; ++dz) {
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (!dx && !dy && !dz) {
          continue;
        }

        int dir[3]{};
        dir[0] = dx;
        dir[1] = dy;
        dir[2] = dz;

        // determine the coordinate and rank of each neighbor
        int nbrcoord[3]{}, nbrRank = -1;
        nbrcoord[0] = mycoord[0] + dir[0];
        nbrcoord[1] = mycoord[1] + dir[1];
        nbrcoord[2] = mycoord[2] + dir[2];
        while (nbrcoord[0] < 0) {
          nbrcoord[0] += dims[0];
        }
        while (nbrcoord[1] < 0) {
          nbrcoord[1] += dims[1];
        }
        while (nbrcoord[2] < 0) {
          nbrcoord[2] += dims[2];
        }
        nbrcoord[0] %= dims[0];
        nbrcoord[1] %= dims[1];
        nbrcoord[2] %= dims[2];
        to_1d(&nbrRank, nbrcoord, dims);

        int weight = halo_size(radius, lcr, dir);
        nbrSendWeight[nbrRank] += weight;
        nbrRecvWeight[nbrRank] += weight;
      }
    }
  }

  // create topology
  MPI_Comm graphComm;
  {
    MPI_Comm comm_old = MPI_COMM_WORLD;
    int indegree = nbrRecvWeight.size();
    std::vector<int> sources;
    std::vector<int> sourceweights;
    for (auto &kv : nbrRecvWeight) {
      sources.push_back(kv.first);
      sourceweights.push_back(kv.second);
    }

    int outdegree = nbrSendWeight.size();
    std::vector<int> destinations;
    std::vector<int> destweights;
    for (auto &kv : nbrSendWeight) {
      destinations.push_back(kv.first);
      destweights.push_back(kv.second);
    }

    MPI_Info info = MPI_INFO_NULL;
    int reorder = 1;
    double start = MPI_Wtime();
    MPI_Dist_graph_create_adjacent(
        comm_old, indegree, sources.data(), sourceweights.data(), outdegree,
        destinations.data(), destweights.data(), info, reorder, &graphComm);
    result.comm.insert(MPI_Wtime() - start);
  }

  // get my new rank after reorder
  MPI_Comm_rank(graphComm, &rank);
  to_3d(mycoord, rank, dims);

  // construct datatypes for each halo direction
  std::map<int, std::vector<MPI_Datatype>> nbrSendType, nbrRecvType;
  for (int dz = -1; dz <= 1; ++dz) {
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (!dx && !dy && !dz) {
          continue;
        }

        int dir[3];
        dir[0] = dx;
        dir[1] = dy;
        dir[2] = dz;

        int nbrcoord[3];
        nbrcoord[0] = mycoord[0] + dir[0];
        nbrcoord[1] = mycoord[1] + dir[1];
        nbrcoord[2] = mycoord[2] + dir[2];

        // wrap neighbors
        while (nbrcoord[0] < 0) {
          nbrcoord[0] += dims[0];
        }
        while (nbrcoord[1] < 0) {
          nbrcoord[1] += dims[1];
        }
        while (nbrcoord[2] < 0) {
          nbrcoord[2] += dims[2];
        }
        nbrcoord[0] %= dims[0];
        nbrcoord[1] %= dims[1];
        nbrcoord[2] %= dims[2];

        int nbrRank;
        to_1d(&nbrRank, nbrcoord, dims);

        MPI_Datatype interior = halo_type(radius, curr, lcr, dir, false);
        MPI_Type_commit(&interior);
        MPI_Datatype exterior = halo_type(radius, curr, lcr, dir, true);
        MPI_Type_commit(&exterior);

        // for periodic, send should always be the same as recv
        {
          int sendSize, recvSize;
          MPI_Type_size(interior, &sendSize);
          MPI_Type_size(exterior, &recvSize);
          assert(sendSize == recvSize);
        }

        nbrSendType[nbrRank].push_back(interior);
        nbrRecvType[nbrRank].push_back(exterior);
      }
    }
  }

  // print neighbors
  for (int r = 0; r < size; ++r) {
    MPI_Barrier(graphComm);
    if (rank == r) {
      std::cout << "rank " << rank << " nbrs= ";
      for (const auto &kv : nbrSendType) {
        std::cout << kv.first << " ";
      }
      std::cout << "\n";
    }
  }
  std::cout << std::flush;

  std::vector<int> sources(nbrRecvType.size(), -1);
  std::vector<int> sourceweights(nbrRecvType.size(), -1);
  std::vector<int> destinations(nbrSendType.size(), -1);
  std::vector<int> destweights(nbrSendType.size(), -1);
  MPI_Dist_graph_neighbors(graphComm, sources.size(), sources.data(),
                           sourceweights.data(), destinations.size(),
                           destinations.data(), destweights.data());

  // print volumes
  for (int r = 0; r < size; ++r) {
    MPI_Barrier(graphComm);
    if (rank == r) {
      std::cout << "rank " << rank << " sizes= ";
      for (const auto &kv : nbrSendType) {
        for (MPI_Datatype ty : kv.second) {
          int size;
          MPI_Type_size(ty, &size);
          std::cout << size << " ";
        }
      }
      std::cout << "\n";
    }
  }
  std::cout << std::flush;

  // print extents
#if 0
  for (int r = 0; r < size; ++r) {
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == r) {
      std::cout << "rank " << rank << " extents= ";
      for (const auto &kv : nbrSendType) {
        for (MPI_Datatype ty : kv.second) {
          MPI_Aint lb, extent;
          MPI_Type_get_extent(ty, &lb, &extent);
          std::cout << "[" << lb << "," << lb + extent << ") ";
        }
      }
      std::cout << "\n";
    }
  }
  std::cout << std::flush;
#endif

  // create the total send/recv buffer
  size_t sendBufSize = 0, recvBufSize = 0;
  for (const auto &kv : nbrSendType) {
    for (MPI_Datatype ty : kv.second) {
      int size;
      MPI_Type_size(ty, &size);
      sendBufSize += size;
    }
  }
  for (const auto &kv : nbrRecvType) {
    for (MPI_Datatype ty : kv.second) {
      int size;
      MPI_Type_size(ty, &size);
      recvBufSize += size;
    }
  }

  char *sendbuf{}, *recvbuf{};
#ifdef USE_CUDA
  CUDA_RUNTIME(cudaMalloc(&sendbuf, sendBufSize));
  CUDA_RUNTIME(cudaMalloc(&recvbuf, recvBufSize));
#else
  sendbuf = new char[sendBufSize];
  recvbuf = new char[recvBufSize];
#endif

// print buffer sizes
#if 1
  std::cout << "rank " << rank << " sendbuf=" << sendBufSize << "\n";
  std::cout << "rank " << rank << " recvbuf=" << recvBufSize << "\n";
  std::cout << std::flush;
#endif

  // pack data for each neighbor and determine offsets in send buffer
  std::vector<int> sendcounts;
  {
    for (int nbr : destinations) {
      int sendcount = 0;
      for (MPI_Datatype ty : nbrSendType[nbr]) {
        int size;
        MPI_Type_size(ty, &size);
        sendcount += size;
      }
      sendcounts.push_back(sendcount);
    }
  }
  std::vector<int> sdispls(sendcounts.size(), 0);
  for (size_t i = 1; i < sdispls.size(); ++i) {
    sdispls[i] = sdispls[i - 1] + sendcounts[i - 1];
  }

#if 1
  std::cerr << "rank " << rank << " sendcounts=";
  for (int e : sendcounts) {
    std::cerr << e << " ";
  }
  std::cerr << "\n";
  std::cerr << "rank " << rank << " sdispls=";
  for (int e : sdispls) {
    std::cerr << e << " ";
  }
  std::cerr << "\n";
#endif

  // determine recv counts and displacements
  std::vector<int> recvcounts;
  for (int nbr : sources) {
    int recvcount = 0;
    for (MPI_Datatype ty : nbrRecvType[nbr]) {
      int size;
      MPI_Type_size(ty, &size);
      recvcount += size;
    }
    recvcounts.push_back(recvcount);
  }
  std::vector<int> rdispls(recvcounts.size(), 0);
  for (size_t i = 1; i < rdispls.size(); ++i) {
    rdispls[i] = rdispls[i - 1] + recvcounts[i - 1];
  }

#if 1
  std::cerr << "rank " << rank << " recvcounts=";
  for (int e : recvcounts) {
    std::cerr << e << " ";
  }
  std::cerr << "\n";
  std::cerr << "rank " << rank << " rdispls=";
  for (int e : rdispls) {
    std::cerr << e << " ";
  }
  std::cerr << "\n";
#endif

  for (int i = 0; i < nIters; ++i) {

    std::cerr << "iter " << i << std::endl;

    // pack the send buf
    {
      nvtxRangePush("pack");
      double start = MPI_Wtime();
      int position = 0;
      for (int nbr : destinations) {
        for (MPI_Datatype ty : nbrSendType[nbr]) {
          MPI_Pack(curr.ptr, 1, ty, sendbuf, sendBufSize, &position, graphComm);
        }
      }
      result.pack.insert(MPI_Wtime() - start);
      nvtxRangePop();
    }

    // exchange
    {
      nvtxRangePush("MPI_Neighbor_alltoallv");
      double start = MPI_Wtime();
      MPI_Neighbor_alltoallv(sendbuf, sendcounts.data(), sdispls.data(),
                             MPI_PACKED, recvbuf, recvcounts.data(),
                             rdispls.data(), MPI_PACKED, graphComm);
      result.alltoallv.insert(MPI_Wtime() - start);
      nvtxRangePop();
    }

    // unpack recv buf
    {
      nvtxRangePush("unpack");
      double start = MPI_Wtime();
      int position = 0;
      for (int nbr : sources) {
        for (MPI_Datatype ty : nbrRecvType[nbr]) {
          MPI_Unpack(recvbuf, recvBufSize, &position, curr.ptr, 1, ty,
                     graphComm);
        }
      }
      result.unpack.insert(MPI_Wtime() - start);
      nvtxRangePop();
    }
  }

  nvtxRangePush("MPI_Comm_free");
  MPI_Comm_free(&graphComm);
  graphComm = {};
  nvtxRangePop();

#ifdef USE_CUDA
  CUDA_RUNTIME(cudaFree(curr.ptr));
  CUDA_RUNTIME(cudaFree(sendbuf));
  CUDA_RUNTIME(cudaFree(recvbuf));
#else
  delete[] curr.ptr;
  delete[] sendbuf;
  delete[] recvbuf;
#endif

  return result;
}

int main(void) {
  MPI_Init(nullptr, nullptr);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  int nIters = 3;
  int nQuants = 2;
  int radius = 2;

  if (0 == rank) {
    std::cout << "comm(s),pack (s),alltoallv (s),unpack (s)\n";
    std::cout << std::flush;
  }

  BenchResult result = bench(MPI_COMM_WORLD, nQuants, radius, nIters);

  double pack, alltoallv, unpack, comm;

  {
    double t1 = result.pack.min();
    double t2 = result.alltoallv.min();
    double t3 = result.unpack.min();
    double t4 = result.comm.min();
    MPI_Reduce(&t1, &pack, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t2, &alltoallv, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t3, &unpack, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t4, &comm, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
  }

  if (0 == rank) {
    std::cout << comm << "," << pack << "," << alltoallv << "," << unpack
              << "\n";
  }

  MPI_Finalize();
}