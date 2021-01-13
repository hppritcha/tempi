//          Copyright Carl Pearson 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//    https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "packer.hpp"

class Packer2D_v2 : public Packer {
  unsigned offset_;
  unsigned blockLength_;
  unsigned count_;
  unsigned stride_;

  int wordSize_; // number of bytes each thread will load
  Dim3 gd_, bd_; // grid dim and block dim for pack kernel

public:
  Packer2D_v2(unsigned off, unsigned blockLength, unsigned count, unsigned stride);
  void pack(void *outbuf, int *position, const void *inbuf,
            const int incount) const override;
  void pack_async(void *outbuf, int *position, const void *inbuf,
                  const int incount) const override;
  void unpack(const void *inbuf, int *position, void *outbuf,
              const int outcount) const override;

  /*public so can be used in benchmarking. If event is not null, record kernel
   * time in event*/
  void launch_pack(void *outbuf, int *position, const void *inbuf,
                   const int incount, cudaStream_t stream,
                   cudaEvent_t kernelStart = {},
                   cudaEvent_t kernelStop = {}) const;
  void launch_unpack(const void *inbuf, int *position, void *outbuf,
                     const int outcount, cudaStream_t stream,
                     cudaEvent_t kernelStart = {},
                     cudaEvent_t kernelStop = {}) const;

private:
};