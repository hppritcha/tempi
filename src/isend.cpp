//          Copyright Carl Pearson 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//    https://www.boost.org/LICENSE_1_0.txt)

#include "isend.hpp"
#include "env.hpp"
#include "logging.hpp"
#include "symbols.hpp"

extern "C" int MPI_Isend(PARAMS_MPI_Isend) {
  if (environment::noTempi) {
    return libmpi.MPI_Isend(ARGS_MPI_Isend);
  }
  return isend::impl(ARGS_MPI_Isend);
}
