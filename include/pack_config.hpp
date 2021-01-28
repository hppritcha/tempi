//          Copyright Carl Pearson 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//    https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cuda_runtime.hpp>

#include <cstdint>

class Pack2DConfig {
  using PackFn = void (*)(void *__restrict__ outbuf,
                          const void *__restrict__ inbuf,
                          const unsigned incount, const unsigned count0,
                          const unsigned count1, const unsigned stride1,
                          const uint64_t extent);

  using UnpackFn = void (*)(void *__restrict__ outbuf,
                            const void *__restrict__ inbuf, const int outcount,
                            unsigned count0, unsigned count1, unsigned stride1,
                            const uint64_t extent);

  dim3 dimGrid;
  dim3 dimBlock;

public:
  PackFn packfn;
  UnpackFn unpackfn;
  Pack2DConfig(unsigned blockLength, unsigned blockCount);

  dim3 dim_grid(int count) const;
  dim3 dim_block() const;
};

class Pack3DConfig {
  using PackFn = void (*)(void *__restrict__ outbuf,
                          const void *__restrict__ inbuf, const int incount,
                          unsigned count0, unsigned count1, unsigned stride1,
                          unsigned count2, unsigned stride2, uint64_t extent);

  using UnpackFn = void (*)(void *__restrict__ outbuf,
                            const void *__restrict__ inbuf, const int incount,
                            unsigned count0, unsigned count1, unsigned stride1,
                            unsigned count2, unsigned stride2, uint64_t extent);

  dim3 dimGrid;
  dim3 dimBlock;

public:
  PackFn packfn;
  UnpackFn unpackfn;
  Pack3DConfig(unsigned blockLength, unsigned count1, unsigned count2);

  const dim3 &dim_grid() const;
  const dim3 &dim_block() const;
};