#include "env.hpp"
#include "logging.hpp"
#include "symbols.hpp"
#include "topology.hpp"

#include <string>

extern "C" int MPI_Neighbor_alltoallv(PARAMS_MPI_Neighbor_alltoallv) {
  if (environment::noTempi) {
    return libmpi.MPI_Neighbor_alltoallv(ARGS_MPI_Alltoallv);
  }

  {

    auto it = degrees.find(comm);
    if (it != degrees.end()) {
      {
        std::string s;
        for (int i = 0; i < it->second.indegree; ++i) {
          s += std::to_string(sendcounts[i]) + " ";
        }
        LOG_SPEW("sendcounts=" << s);
      }

      {
        std::string s;
        for (int i = 0; i < it->second.outdegree; ++i) {
          s += std::to_string(recvcounts[i]) + " ";
        }
        LOG_SPEW("recvcounts=" << s);
      }
    }
  }

  return libmpi.MPI_Neighbor_alltoallv(ARGS_MPI_Alltoallv);
}
