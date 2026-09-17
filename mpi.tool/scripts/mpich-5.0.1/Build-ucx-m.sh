#!/bin/sh

(

. env.sh
unset CC
unset CXX
unset FC
unset MPICC
unset MPICXX
unset MPIFC


./configure  --prefix=mpich/5.0.1x-mt-ucx --enable-silent-rules --enable-fast=all --enable-yield=sched_yield \
    --with-device=ch4:ucx:shm --with-pm=hydra --enable-romio  --with-ch3-rank-bits=32 --enable-threads=multiple --without-ze --with-file-system=lustre+nfs \
    --enable-shared --with-hwloc=embedded  --with-ucx=embedded --with-libfabric=$HOME/libfabric --enable-fortran=all --with-ch4-shmmods=posix \
   CC=icx F77=ifx FC=ifx CXX=icpx \
   MPICHLIB_CPPFLAGS="-I$WORKINIT/misc.libs/lustre-release/lustre/include -I$WORKINIT/misc.libs/lustre-release/lustre/include/uapi" \
   MPICHLIB_CFLAGS='-Wno-unused-but-set-variable -Wno-tautological-constant-compare -Wno-initializer-overrides -fno-omit-frame-pointer' \
   MPICHLIB_FCFLAGS='-fno-omit-frame-pointer' \
   MPICHLIB_CXXFLAGS='-Wno-unused-but-set-variable -Wno-tautological-constant-compare -Wno-initializer-overrides -fno-omit-frame-pointer' \
   2>&1 | tee configure.log

exit_code=`tail -n 1 config.log | grep "exit" `
echo configure done with $exit_code

sleep 10
if [ -n "$exit_code" ] ; then
make clean
make -j 16 2>&1 | tee make.log
make install  2>&1 | tee -a make.log
fi


)

(

. env.sh
unset CC
unset CXX
unset FC
unset MPICC
unset MPICXX
unset MPIFC


./configure  --prefix=mpich/5.0.1g-mt-ucx --enable-silent-rules --enable-fast=all --enable-yield=sched_yield \
    --with-device=ch4:ucx:shm --with-pm=hydra --enable-romio  --with-ch3-rank-bits=32 --enable-threads=multiple --without-ze --with-file-system=lustre+nfs \
    --enable-shared --with-hwloc=embedded  --with-ucx=embedded --with-libfabric=$HOME/libfabric --enable-fortran=all --with-ch4-shmmods=posix \
   CC=gcc F77=gfortran FC=gfortran CXX=g++ \
   MPICHLIB_CPPFLAGS="-I$WORKINIT/misc.libs/lustre-release/lustre/include -I$WORKINIT/misc.libs/lustre-release/lustre/include/uapi" \
   MPICHLIB_CFLAGS='-Wno-unused-but-set-variable -Wno-tautological-constant-compare -Wno-initializer-overrides -fno-omit-frame-pointer' \
   MPICHLIB_FCFLAGS='-fno-omit-frame-pointer' \
   MPICHLIB_CXXFLAGS='-Wno-unused-but-set-variable -Wno-tautological-constant-compare -Wno-initializer-overrides -fno-omit-frame-pointer' \
   2>&1 | tee configure.log

exit_code=`tail -n 1 config.log | grep "exit" `
echo configure done with $exit_code

sleep 10
if [ -n "$exit_code" ] ; then
make clean
make -j 16 2>&1 | tee make.log
make install  2>&1 | tee -a make.log
fi

)
