#!/bin/sh


mpi_type=$1

if [ -z "$3" ] ; then 
DIAG=off
else
DIAG=$3
fi

if [ -z "$4" ] ; then 
PARSE_GPU=off
else
PARSE_GPU=$4
fi


. env.sh

if [ -z "$ENG_MPI_BUILD" ] ; then 
ENG_MPI_BUILD=`expr "$mpi_version" : '.*\(eng\)'`
fi

if [ -n "$ENG_MPI_BUILD" ] ; then 
mpi_internal="PARSE_MPI_INTERNAL=on";
libfabric_eng=-eng
fi
export LD_LIBRARY_PATH=$HOME/libfabric${libfabric_eng}/lib:$LD_LIBRARY_PATH # need path pointed to correct libfabric 

TARGET=$2

src_tmp="src-TARGET=${TARGET}-ENG_MPI_BUILD=${ENG_MPI_BUILD}-DIAG=${DIAG}-PARSE_GPU=${PARSE_GPU}"

echo $src_tmp >>Build.${COMPILER}.dir

if [ -n "$do_not_remove" ] ; then
rm -r $src_tmp
fi

if [ ! -d $src_tmp ] ; then
mkdir -p $src_tmp
else
echo "Directory $src_tmp exists...exiting"
exit
fi
cp -rv src/* $src_tmp
cd $src_tmp
make clean $mpi_internal OMPT=on TARGET=$TARGET DIAG=$DIAG

set -x

make COMPILER=$COMPILER clean $mpi_internal OMPT=on PARSE_GPU=$PARSE_GPU TARGET=$TARGET DIAG=$DIAG 2>&1 
make -j 4 COMPILER=$COMPILER $mpi_internal OMPT=on PARSE_GPU=$PARSE_GPU TARGET=$TARGET DIAG=$DIAG 2>&1 | tee ./Build.log
make COMPILER=$COMPILER install $mpi_internal OMPT=on PARSE_GPU=$PARSE_GPU TARGET=$TARGET DIAG=$DIAG 2>&1 | tee -a ./Build.log
echo "Log From $src_tmp" >>../Build.${COMPILER}.log
cat Build.log >>../Build.${COMPILER}.log
cd -

if [ -z "$do_not_remove" ] ; then
rm -r $src_tmp
fi