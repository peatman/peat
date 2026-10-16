#!/bin/sh

rm -f Build.log

./Build.sh oneAPI:2026.1.0 impi impi #also suitable for mpich and mvapich
#./Build.sh oneAPI:2026.1.0 openmpi:5.0.10x  ompi 
./Build.sh gcc openmpi:5.0.10g  ompi 

./Build.sh gcc  none serial 
