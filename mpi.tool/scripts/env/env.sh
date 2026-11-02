#!/bin/sh
if [ -z "$WORKINIT" ] ; then
WORKINIT=/home/$USER
fi

if [ -z "$HOMEINIT" ] ; then
HOMEINIT=/home/$USER
fi

if [ -z "$compiler" ] ; then
ctype=oneapi
cversion=2026.1.0 
else
ctype=`echo $compiler | cut -f 1 -d:`
cversion=`echo $compiler | cut -s -f 2 -d:`
# compiler=nvc:<version>[:<version>] (NVHPC release and/or CUDA version, see the nvc branch below)
nversion=`echo $compiler | cut -s -f 3 -d:`
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
elif [ "$ctype" == "nvc" -o "$ctype" == "nvhpc" ] ; then
NVHPC_ROOT_PATH=${NVHPC_ROOT_PATH:-/opt/nvidia/hpc_sdk}
nvhpc_version=
cuda_version=
for v in $cversion $nversion ; do
  if [ -d "$NVHPC_ROOT_PATH/Linux_x86_64/$v/compilers/bin" ] ; then
    nvhpc_version=$v
  elif [ -d "/opt/cuda-$v" -o -d "/usr/local/cuda-$v" ] ; then
    cuda_version=$v
  else
    echo "env.sh: '$v' is neither an NVHPC release under $NVHPC_ROOT_PATH/Linux_x86_64 nor a CUDA toolkit (/opt/cuda-$v, /usr/local/cuda-$v)" >&2
  fi
done
if [ -z "$nvhpc_version" ] ; then
  nvhpc_version=`ls -1 $NVHPC_ROOT_PATH/Linux_x86_64 2>/dev/null | grep -E '^[0-9]' | sort -V | tail -1`
fi
if [ -d "$NVHPC_ROOT_PATH/Linux_x86_64/$nvhpc_version/compilers/bin" ] ; then
  export NVHPC=$NVHPC_ROOT_PATH
  export PATH=$NVHPC_ROOT_PATH/Linux_x86_64/$nvhpc_version/compilers/bin:$PATH
  export LD_LIBRARY_PATH=$NVHPC_ROOT_PATH/Linux_x86_64/$nvhpc_version/compilers/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
else
  echo "env.sh: no NVHPC found under $NVHPC_ROOT_PATH/Linux_x86_64" >&2
fi
if [ -n "$cuda_version" ] ; then
  for CUDA_ROOT_PATH in /opt/cuda-$cuda_version /usr/local/cuda-$cuda_version ; do
    if [ -d "$CUDA_ROOT_PATH" ] ; then
      export NVHPC_CUDA_HOME=$CUDA_ROOT_PATH
      break
    fi
  done
fi
[ -d /usr/lib/wsl/lib ] && export LD_LIBRARY_PATH=/usr/lib/wsl/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export MPICH_CC=nvc
export MPICH_CXX=nvc++
export MPICH_FC=nvfortran
# the same for the other MPIs: Open MPI and Intel MPI wrappers
export OMPI_CC=nvc
export OMPI_CXX=nvc++
export OMPI_FC=nvfortran
export I_MPI_CC=nvc
export I_MPI_CXX=nvc++
export I_MPI_FC=nvfortran
export CC=nvc
export CXX=nvc++
export FC=nvfortran
else
COMPILER_ROOT_PATHS="$HOME/gcc"
if [ -z "$cversion" ] ; then 
  cversion=14.1.0  
fi 
for COMPILER_ROOT_PATH in $COMPILER_ROOT_PATHS ; do
if [ -e "$COMPILER_ROOT_PATH/${cversion}/bin/gccvars.sh" ] ; then
. $COMPILER_ROOT_PATH/${cversion}/bin/gccvars.sh 
break
fi
done
export CC=gcc
export CXX=g++
export FC=gfortran
if [ "$ctype" == "clang" ] ; then
# system-wide clang (one version), handled like gcc; no Fortran front end, gfortran stays. As for nvc, the MPI wrappers
# are pointed to it (MPICH_*, OMPI_*, I_MPI_*).
export CC=clang
export CXX=clang++
export MPICH_CC=clang
export MPICH_CXX=clang++
export MPICH_FC=gfortran
export OMPI_CC=clang
export OMPI_CXX=clang++
export OMPI_FC=gfortran
export I_MPI_CC=clang
export I_MPI_CXX=clang++
export I_MPI_FC=gfortran
fi

fi


if [ "x-$mpi_type" == "x-impi" ] ; then 
if [ -z "$I_MPI_ROOT" ] ;  then 
# mo mpi - compiler presumably not intel
. /opt/intel/oneapi/2026.1.0/mpi/latest/env/vars.sh #system specific
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
  export IPMMPI=openmpi 

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

  . $HOMEINIT/mvapich/$mpi_version/bin/mpivars.sh
  export MPIF=$MVAPICH_ROOT/include
  export MPICC=mpicc
  export MPIFC=mpif90
  export MPICXX=mpicxx
elif [ "x-$mpi_type" == "x-mpich" ] 
  then
  if [ -z "$mpi_version" ] ;  then
case "$CC" in
    icx ) 
     export mpi_version=4.3.1x-mt-sock 
    ;;
    * )
     export mpi_version=4.3.1g-mt-sock
    ;;
esac
  else 
   export mpi_version
  fi

  . $HOMEINIT/mpich/$mpi_version/bin/mpivars.sh
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

export OFIPATH=$HOME/libfabric${libfabric_eng}
export UCXPATH=$HOME/ucx
export PSM2PATH=$HOME/psm2

export LD_LIBRARY_PATH=$OFIPATH/lib:$UCXPATH/lib:$PSM2PATH/lib64:$HOME/psm3/lib/libfabric:$LD_LIBRARY_PATH
export FI_PROVIDER_PATH=$OFIPATH/lib
