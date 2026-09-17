#!/bin/sh

#. /opt/intel/Compiler/2013.5.192/bin/compilervars.sh intel64
#. /opt/intel/mpi/4.1.2.038/bin64/mpivars.sh
#export PAPI_ROOT=/opt/crtdc/hwr/tools/The.M.Tool/PAPI5.3
#ENG_IMPI_BUILD=yes
compiler=$1
mpi_type=$2
. env.sh

make clean TARGET=$3 
make -j 16 TARGET=$3  2>&1 | tee -a Build.log
make install TARGET=$3 
