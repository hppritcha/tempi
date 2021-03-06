//          Copyright Carl Pearson 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//    https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "symbols.hpp"

/* implement as a bunch of Isend/Irecv, with the remote ones issued before the
 * local ones
 */
int alltoallv_isir_remote_first(PARAMS_MPI_Alltoallv);

/* implement as a bunch of copies to the CPU, then a CPU alltoallv, then a bunch
 * of copies to the GPU
 */
int alltoallv_staged(PARAMS_MPI_Alltoallv);

/* copy to host, and do a bunch of isend/irecv
 */
int alltoallv_isir_staged(PARAMS_MPI_Alltoallv);

/* implement as isends/irecvs.
   local is cuda-aware, remote is staged through host
 */
int alltoallv_isir_remote_staged(PARAMS_MPI_Alltoallv);