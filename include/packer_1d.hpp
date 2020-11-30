#pragma once

#include "packer.hpp"

class Packer1D : public Packer {
  unsigned offset_;
  unsigned extent_;

public:
  Packer1D(unsigned off, unsigned extent);
  void pack(void *outbuf, int *position, const void *inbuf,
            const int incount) const override;
  void unpack(const void *inbuf, int *position, void *outbuf,
              const int outcount) const override;

private:
  void launch_pack(void *outbuf, int *position, const void *inbuf,
            const int incount, cudaStream_t stream) const;
  void launch_unpack(const void *inbuf, int *position, void *outbuf,
              const int outcount, cudaStream_t stream) const;
};