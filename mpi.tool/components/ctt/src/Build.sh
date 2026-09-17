#!/bin/sh 

#. /opt/intel/Compiler/2013.5.192/bin/compilervars.sh intel64
#. /opt/intel/mpi/4.1.2.038/bin64/mpivars.sh
#export PAPI_ROOT=/opt/crtdc/hwr/tools/The.M.Tool/PAPI5.3
#ENG_IMPI_BUILD=yes
#compiler=gcc:14.1.0 #oneAPI:2025.2.0
#. env.sh

make clean TARGET=$1 
make -j 16 TARGET=$1  2>&1 | tee Build.log
make install TARGET=$1 
