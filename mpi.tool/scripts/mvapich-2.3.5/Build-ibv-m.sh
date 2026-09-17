#!/bin/sh
#compiler=il0:2020u1
#compiler=oneAPI:2023.2.0.49397
compiler=oneAPI:2026.1.0

. env.sh
./configure  --prefix=$HOME/mvapich/2.3.5x-mt-ib-verbs --enable-silent-rules --enable-shared \
    --with-device=ch3:mrail --with-pm=hydra --enable-romio  --with-ch3-rank-bits=32 --enable-threads=multiple \
    --enable-xrc --disable-ibv-dlopen --disable-opencl \
    --with-file-system=lustre+nfs --enable-yield=sched_yield --enable-fortran=all \
   CC=icx F77=ifx FC=ifx CXX=icpx \
   CPPFLAGS='-I$HOME/misc.libs/lustre-release/lustre/include -I$HOME/misc.libs/lustre-release/lustre/include/uapi' \
   MPICHLIB_CFLAGS='-O3 -fno-omit-frame-pointer -Wno-incompatible-function-pointer-types -Wno-implicit-function-declaration -DUSE_MPI_VERSIONS' \
   MPICHLIB_CXXFLAGS='-O3 -fno-omit-frame-pointer -Wno-incompatible-function-pointer-types -Wno-implicit-function-declaration -DUSE_MPI_VERSIONS' \
   MPICHLIB_FCFLAGS='-O3 -fno-omit-frame-pointer -DUSE_MPI_VERSIONS' \
   MPICHLIB_FFLAGS='-O3 -fno-omit-frame-pointer -DUSE_MPI_VERSIONS' \
   2>&1 | tee configure-ibv-m.log
#    --with-ib-include=/global/panfs01/users/aknyaze1/ofed/mlnx/include --with-ib-libpath=/usr/local/ofed/CURRENT/lib64  \
exit_code=`tail -n 1 config.log | grep "exit 0" `
echo configure done with $exit_code
sleep 10
if [ -n "$exit_code" ] ; then
make clean
make -j 16 2>&1 | tee make-ibv-m.log
make install  2>&1 | tee -a make-ibv-m.log
fi

