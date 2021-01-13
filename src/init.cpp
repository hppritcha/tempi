//          Copyright Carl Pearson 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//    https://www.boost.org/LICENSE_1_0.txt)

#include "allocators.hpp"
#include "env.hpp"
#include "logging.hpp"
#include "measure_system.hpp"
#include "streams.hpp"
#include "symbols.hpp"
#include "topology.hpp"
#include "types.hpp"
#include "worker.hpp"

#include <mpi.h>

#include <nvToolsExt.h>

extern "C" int MPI_Init(PARAMS_MPI_Init) {

  // before anything else, read env vars to control tempi
  init_symbols();
  read_environment();
  static Func_MPI_Init fn = libmpi.MPI_Init;
  if (environment::noTempi) {
    return fn(ARGS_MPI_Init);
  }

  int err = fn(ARGS_MPI_Init);
  // can use logging now that MPI_Init has been called
  LOG_SPEW("finished library MPI_Init");

  int provided;
  MPI_Query_thread(&provided);
  if (MPI_THREAD_SINGLE == provided) {
    LOG_SPEW("MPI_THREAD_SINGLE");
  } else if (MPI_THREAD_FUNNELED == provided) {
    LOG_SPEW("MPI_THREAD_FUNNELED");
  } else if (MPI_THREAD_SERIALIZED == provided) {
    LOG_SPEW("MPI_THREAD_SERIALIZED");
  } else if (MPI_THREAD_MULTIPLE == provided) {
    LOG_SPEW("MPI_THREAD_MULTIPLE");
  }

  allocators::init();
  topology_init();
  streams_init();
  worker_init();
  types_init();
  measure_system_init();

  return err;
}
