#!/bin/sh

if [ -z "$WORKINIT" ] ; then
WORKINIT=/home/$USER
fi

if [ -z "$HOMEINIT" ] ; then
HOMEINIT=/home/$USER
fi

if [ -z "$compiler" ] ; then
ctype=oneapi
#cversion=2023.2.0.49397
#cversion=2025.1.0
cversion=2026.1.0 
else
ctype=`echo $compiler | cut -f 1 -d:`
cversion=`echo $compiler | cut -s -f 2 -d:`
fi

if [ -z "$mpi_type" ] ;  then
export mpi_type=impi
fi

if [ -z "$ia_arch" ] ;  then
ia_arch=intel64
fi


colon=`expr index "$mpi_type" :`
if [ $colon -gt 0 ]; then 
export mpi_version=`echo $mpi_type | cut -s -f 2 -d:`
export mpi_type=`echo $mpi_type | cut -f 1 -d:`
# we'll set mpi_version later
fi

#if [ -z "$mpi_version" ] ;  then
#fi

export cpu_arch03=${cpu_arch:0:3}
if [ "$ctype" == "oneAPI" -o "$ctype" == "oneapi" ] ; then
COMPILER_ROOT_PATHS="/opt/intel/oneapi"
if [ -z "$cversion" ] ; then
cversion=latest
fi

for COMPILER_ROOT_PATH in $COMPILER_ROOT_PATHS ; do
if [ -e "$COMPILER_ROOT_PATH/${cversion}/setvars.sh" ] ; then
. $COMPILER_ROOT_PATH/${cversion}/setvars.sh --force
a=`which icc 2>/dev/null`
b=`which ifort 2>/dev/null`
c=`which icpc 2>/dev/null`

if [ -n "$a" ] ; then
export CC=icc
else
export CC=icx
fi
if [ -n "$b" ] ; then
export FC=ifort
else
export FC=ifx
fi
if [ -n "$c" ] ; then
export CXX=icpc
else
export CXX=icpx
fi
break
fi
done
else
COMPILER_ROOT_PATHS="$HOMEINIT/gcc"
if [ -z "$cversion" ] ; then 
  cversion=14.1.0  
fi 
for COMPILER_ROOT_PATH in $COMPILER_ROOT_PATHS ; do
if [ -e "$COMPILER_ROOT_PATH/${cversion}/bin/gccvars.sh" ] ; then
. $COMPILER_ROOT_PATH/${cversion}/bin/gccvars.sh #since it's build from source it has to be there
break
fi
done
export CC=gcc
export CXX=g++
export FC=gfortran

fi


if [ "x-$mpi_type" == "x-impi" ] ; then 
if [ -z "$I_MPI_ROOT" ] ;  then 
# mo mpi - compiler presumably not intel
. /opt/intel/oneapi/mpi/latest/env/vars.sh
  export MPICC=mpicc
  export MPICXX=mpicxx
  export MPIFC=mpif90
else
# I_MPI_ROOT already set (via oneapi setvars) compiler presumably intel
  export MPICC=mpi${CC}
  export MPICXX=mpi${CXX}
  export MPIFC=mpi${FC}
fi

#fi
  export MKL_BLACS=$MKLLIB/libmkl_blacs_intelmpi20_lp64.a 
  export ITAC_ROOT=/home/opt/intel/lrb/itac/8.1.0.024
elif [ "x-$mpi_type" == "x-openmpi" ] ;  then
if [ -z "$mpi_version" ] ;  then
export mpi_version=5.0.10g
else 
export mpi_version
fi

  export PATH=$HOMEINIT/openmpi/$mpi_version/bin:$PATH
  export LD_LIBRARY_PATH=$HOMEINIT/openmpi/$mpi_version/lib64:$HOMEINIT/openmpi/$mpi_version/lib64/openmpi:$LD_LIBRARY_PATH
  export MPIF=$HOMEINIT/openmpi/$mpi_version/include
  export GMS_MPI_PATH=$HOMEINIT/openmpi/$mpi_version
  export MPICC=mpicc
  export MPIFC=mpif90
  export MPICXX=mpicxx
#  export comm=mixed
#  export comm=mpi
  export IPMMPI=openmpi 
#  export MKL_BLACS=$MKLLIB/libmkl_blacs_lp64.a 
  export MPI2SUPPORT=-DMPI2_SUPPORT

elif [ "x-$mpi_type" == "x-mvapich" ] 
  then
  if [ -z "$mpi_version" ] ;  then
  if [ -n "$ENG_BUILDS" ] ; then
   export mpi_version=2.3.5-st-ib-verbs-eng 
  else
#  export mpi_version=2.3.7-st-ib-verbs
   export mpi_version=4.1g-mt-sock-eng
  fi
  else 
  export mpi_version
  fi

  . $HOMEINIT/mvapich/$mpi_version/bin/mpivars.sh  #since it's build from source it has to be there
  export MPIF=$MVAPICH_ROOT/include
  export MPICC=mpicc
  export MPIFC=mpif90
  export MPICXX=mpicxx
elif [ "x-$mpi_type" == "x-mpich" ] 
  then
  if [ -z "$mpi_version" ] ;  then
case "$CC" in
    icx ) 
     export mpi_version=4.3.2x-mt-ofi 
    ;;
    * )
     export mpi_version=4.3.2g-mt-ofi
    ;;
esac
  else 
   export mpi_version
  fi

  . $HOMEINIT/mpich/$mpi_version/bin/mpivars.sh #since it's build from source it has to be there
  export MPIF=$MPICH_ROOT/include
  export GMS_MPI_PATH=$MPICH_ROOT

  export MPICC=mpicc
  export MPIFC=mpifort
  export MPICXX=mpicxx
  export GMS_MPI_LIB=mpich
  export IPMMPI=mpich 
fi

if [ "$MPICXX" == "mpig++" ] ; then
    export MPICXX="mpigxx"
fi

export LD_LIBRARY_PATH=$HOMEINIT/libfabric/lib:$HOMEINIT/ucx/lib:$HOMEINIT/psm2/lib64:$HOMEINIT/psm3/lib/libfabric:$LD_LIBRARY_PATH
