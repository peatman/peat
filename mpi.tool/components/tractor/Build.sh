#!/bin/sh


mpi_type=$1

if [ -z "$4" ] ; then 
DIAG=off
else
DIAG=$4
fi

if [ -z "$5" ] ; then 
PARSE_GPU=off
else
PARSE_GPU=$5
fi

if [ -z "$6" ] ; then 
HWCOUNTERS=off
else
HWCOUNTERS=$6
fi


if [ -z "$7" ] ; then 
MPIBUFFERS=off
else
MPIBUFFERS=$7
fi

if [ -z "$8" ] ; then 
VIRT2PHYS=off
else
VIRT2PHYS=$8
fi


. env.sh

if [ -z "$ENG_MPI_BUILD" ] ; then 
ENG_MPI_BUILD=`expr "$mpi_version" : '.*\(eng\)'`
fi

if [ -n "$ENG_MPI_BUILD" ] ; then 
colt="PARSE_COLT=on";
libfabric_eng=-eng
fi

export LD_LIBRARY_PATH=$HOME/libfabric${libfabric_eng}/lib:$LD_LIBRARY_PATH # need path pointed to correct libfabric 

TARGET=$2
MEMPROFILE=$3 #on/off

src_tmp="src-TARGET=${TARGET}-ENG_MPI_BUILD=${ENG_MPI_BUILD}-MEMPROFILE=${MEMPROFILE}-HWCOUNTERS=${HWCOUNTERS}-MPIBUFFERS=${MPIBUFFERS}-DIAG=${DIAG}-VIRT2PHYS=${VIRT2PHYS}-PARSE_GPU=${PARSE_GPU}"

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
make clean $colt OMPT=on TARGET=$TARGET MEMPROFILE=$MEMPROFILE HWCOUNTERS=$HWCOUNTERS MPIBUFFERS=$MPIBUFFERS DIAG=$DIAG VIRT2PHYS=$VIRT2PHYS



set -x

make COMPILER=$COMPILER clean $colt OMPT=on PARSE_GPU=$PARSE_GPU TARGET=$TARGET MEMPROFILE=$MEMPROFILE HWCOUNTERS=$HWCOUNTERS MPIBUFFERS=$MPIBUFFERS DIAG=$DIAG VIRT2PHYS=$VIRT2PHYS 2>&1 
make -j 4 COMPILER=$COMPILER $colt OMPT=on PARSE_GPU=$PARSE_GPU TARGET=$TARGET MEMPROFILE=$MEMPROFILE HWCOUNTERS=$HWCOUNTERS MPIBUFFERS=$MPIBUFFERS DIAG=$DIAG VIRT2PHYS=$VIRT2PHYS 2>&1 | tee ./Build.log
make COMPILER=$COMPILER install $colt OMPT=on PARSE_GPU=$PARSE_GPU TARGET=$TARGET MEMPROFILE=$MEMPROFILE HWCOUNTERS=$HWCOUNTERS MPIBUFFERS=$MPIBUFFERS DIAG=$DIAG VIRT2PHYS=$VIRT2PHYS 2>&1 | tee -a ./Build.log
echo "Log From $src_tmp" >>../Build.${COMPILER}.log
cat Build.log >>../Build.${COMPILER}.log
cd -

if [ -z "$do_not_remove" ] ; then
rm -r $src_tmp
fi