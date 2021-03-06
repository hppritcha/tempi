//          Copyright Carl Pearson 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//    https://www.boost.org/LICENSE_1_0.txt)

#include "packer.hpp"

#include "cuda_runtime.hpp"
#include "logging.hpp"
#include "streams.hpp"

Packer::LaunchInfo Packer::pack_launch_info(const void *inbuf) {
  (void) inbuf;
  Packer::LaunchInfo ret{.stream = kernStream};
  return ret;
}

Packer::LaunchInfo Packer::unpack_launch_info(const void *outbuf) {
  (void) outbuf;
  Packer::LaunchInfo ret{.stream = kernStream};
  return ret;
}

#if 0
void Packer::sync(const void *inbuf) {
  Packer::LaunchInfo info = pack_launch_info(inbuf);
  if (!info.stream) {
    LOG_ERROR("requested Packer sync for "
              << uintptr_t(inbuf)
              << " which does not have an associated stream");
  } else {
    CUDA_RUNTIME(cudaStreamSynchronize(info.stream));
  }
}
#endif