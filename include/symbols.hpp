#pragma once

#include <mpi.h>

#define PARAMS_MPI_Init int *argc, char ***argv
#define ARGS_MPI_Init argc, argv
typedef int (*Func_MPI_Init)(PARAMS_MPI_Init);

#define PARAMS_MPI_Init_thread                                                 \
  int *argc, char ***argv, int required, int *provided
#define ARGS_MPI_Init_thread argc, argv, required, provided
typedef int (*Func_MPI_Init_thread)(PARAMS_MPI_Init_thread);

#define PARAMS_MPI_Isend                                                       \
  const void *buf, int count, MPI_Datatype datatype, int dest, int tag,        \
      MPI_Comm comm, MPI_Request *request
#define ARGS_MPI_Isend buf, count, datatype, dest, tag, comm, request
typedef int (*Func_MPI_Isend)(PARAMS_MPI_Isend);

struct MpiFunc {
  Func_MPI_Init MPI_Init;
  Func_MPI_Init_thread MPI_Init_thread;
  Func_MPI_Isend MPI_Isend;
};

extern MpiFunc libmpi;

// load all MPI symbols
void init_symbols();