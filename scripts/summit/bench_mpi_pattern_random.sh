#!/bin/bash
#BSUB -P csc362
#BSUB -J bench_mpi_pattern_random 
#BSUB -o bench_mpi_pattern_random.o%J
#BSUB -e bench_mpi_pattern_random.e%J
#BSUB -W 01:00
#BSUB -nnodes 32

set -eou pipefail

module reset
module unload darshan-runtime
module load gcc
module load cuda/11.0.3
module load nsight-systems/2020.3.1.71

SCRATCH=/gpfs/alpine/scratch/cpearson/csc362/tempi_results
OUT=$SCRATCH/bench_mpi_pattern_random.csv

set -x

mkdir -p $SCRATCH

export TEMPI_PLACEMENT_KAHIP=""
export TEMPI_NO_ALLTOALLV=""

#echo "2nodes,6rankpernode" >> $OUT
#jsrun --smpiargs="-gpu" -n 12 -r 6 -a 1 -g 1 -c 7 -b rs \
#nsys profile -t cuda,nvtx -f true -o $SCRATCH/tempi_pattern_2n_12r_%q{OMPI_COMM_WORLD_RANK}.qdrep ../../build/bin/bench-mpi-pattern-random | tee -a $OUT

echo "" > $OUT

for nodes in 32; do
  for rpn in 2 6; do
    let n=$rpn*$nodes

    echo ${nodes}nodes,${rpn}rankspernode,tempi >> $OUT
    unset TEMPI_DISABLE
    jsrun --smpiargs="-gpu" -n $n -r $rpn -a 1 -g 1 -c 7 -b rs ../../build/bin/bench-mpi-random-neighbor-alltoallv | tee -a $OUT

    echo ${nodes}nodes,${rpn}rankspernode,notempi >> $OUT
    export TEMPI_DISABLE=""
    jsrun --smpiargs="-gpu" -n $n -r $rpn -a 1 -g 1 -c 7 -b rs ../../build/bin/bench-mpi-random-alltoallv          | tee -a $OUT
    jsrun --smpiargs="-gpu" -n $n -r $rpn -a 1 -g 1 -c 7 -b rs ../../build/bin/bench-mpi-random-isend-irecv        | tee -a $OUT
    jsrun --smpiargs="-gpu" -n $n -r $rpn -a 1 -g 1 -c 7 -b rs ../../build/bin/bench-mpi-random-sparse-isend-irecv | tee -a $OUT
    jsrun --smpiargs="-gpu" -n $n -r $rpn -a 1 -g 1 -c 7 -b rs ../../build/bin/bench-mpi-random-neighbor-alltoallv | tee -a $OUT
  done
done

