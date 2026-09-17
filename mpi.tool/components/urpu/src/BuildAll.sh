#!/bin/sh
rm -f Build.log
#./Build.sh oneapi:2026.1.0 mvapich2:2.3.5-st-ib-verbs-eng  mvapich2 
./Build.sh oneapi:2026.1.0 impi impi #also suitable for mpich and mvapich
./Build.sh gcc openmpi:5.0.10g ompi
./Build.sh gcc stub  serial 
