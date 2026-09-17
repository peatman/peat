#!/bin/sh
set -x
base=$(dirname $(readlink -e ${BASH_SOURCE[0]}))

export MPI_ROOT=$(dirname $(readlink -e ${base}))
export PATH=${MPI_ROOT}/bin${PATH:+:$PATH}
export LD_LIBRARY_PATH=${MPI_ROOT}/lib64:${MPI_ROOT}/lib:/usr/local/ofed/CURRENT/lib64/libibverbs:$LD_LIBRARY_PATH${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}


if [ -z "$MPICH_CC" ] ; then 
#MP_CC=`cat ${MPI_ROOT}/bin/mpicc | grep 'CC=' | head -1 | sed -e 's/"//g'` 
MP_CC=`cat ${MPI_ROOT}/bin/mpicc | grep 'CC=' | head -1 | sed -e 's/^.*="//g' -e 's/"//g'` 
if [ -n "$MP_CC" ] ; then
#export $MP_CC
export CC="$MP_CC"
fi
else
export CC=$MPICH_CC
fi

if [ -z "$MPICH_CXX" ] ; then 
#MP_CXX=`cat ${MPI_ROOT}/bin/mpicxx | grep 'CXX=' | head -1 | sed -e 's/"//g'`
MP_CXX=`cat ${MPI_ROOT}/bin/mpicxx | grep 'CXX=' | head -1 | sed -e 's/^.*="//g' -e 's/"//g'` 
if [ -n "$MP_CXX" ] ; then
#export $MP_CXX
export CXX="$MP_CXX"
fi
else
export CXX=$MPICH_CXX
fi

if [ -z "$MPICH_FC" ] ; then 
#MP_FC=`cat ${MPI_ROOT}/bin/mpif90 | grep 'FC=' | head -1 | sed -e 's/"//g'`
MP_FC=`cat ${MPI_ROOT}/bin/mpif90 | grep 'FC=' | head -1 | sed -e 's/^.*="//g' -e 's/"//g'` 
if [ -n "$MP_FC" ] ; then
#export $MP_FC
export FC="$MP_FC"
fi
else
export FC=$MPICH_FC
fi
#tempotal solution

unset HYDRA_LAUNCHER_EXTRA_ARGS

