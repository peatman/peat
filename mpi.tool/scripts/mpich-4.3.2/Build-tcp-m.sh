#!/bin/sh

(
compiler=oneAPI:2026.1.0

. env.sh
unset CC
unset CXX
unset FC
unset MPICC
unset MPICXX
unset MPIFC


./configure  --prefix=$HOME/mpich/4.3.2x-mt-sock --enable-silent-rules --enable-fast=all \
    --with-device=ch3:sock --with-pm=hydra --enable-romio  --with-ch3-rank-bits=32 --enable-threads=multiple --without-ze --with-file-system=lustre+nfs \
    --enable-shared --with-hwloc=embedded  --with-ucx=embedded --with-libfabric=embedded --enable-fortran=all --with-ch4-shmmods=posix \
   CC=icx F77=ifx FC=ifx CXX=icpx \
   MPICHLIB_CPPFLAGS="-I$HOME/misc.libs/lustre-release/lustre/include -I$HOME/misc.libs/lustre-release/lustre/include/uapi" \
   MPICHLIB_CFLAGS='-Wno-unused-but-set-variable -Wno-tautological-constant-compare -Wno-initializer-overrides -fno-omit-frame-pointer' \
   MPICHLIB_FCFLAGS='-fno-omit-frame-pointer' \
   MPICHLIB_CXXFLAGS='-Wno-unused-but-set-variable -Wno-tautological-constant-compare -Wno-initializer-overrides -fno-omit-frame-pointer' \
   2>&1 | tee configure.log
#   LDFLAGS="-L$HOMEINIT/openshmem/lib64 -L$HOMEINIT/openpmix/lib64" \
#    --with-ib-include=/global/panfs01/users/aknyaze1/ofed/mlnx/include --with-ib-libpath=/usr/local/ofed/CURRENT/lib64  \
#--with-pic=yes
#   --with-ucx=$HOMEINIT/ucx   \


exit_code=`tail -n 1 config.log | grep "exit 0" `
echo configure done with $exit_code
sleep 10
if [ -n "$exit_code" ] ; then
make clean
make -j 16 2>&1 | tee make.log
make install  2>&1 | tee -a make.log
fi

)
(

unset CC
unset CXX
unset FC
unset MPICC
unset MPICXX
unset MPIFC

./configure  --prefix=$HOME/mpich/4.3.2g-mt-sock --enable-silent-rules --enable-fast=all \
    --with-device=ch3:sock --with-pm=hydra --enable-romio  --with-ch3-rank-bits=32 --enable-threads=multiple --without-ze --with-file-system=lustre+nfs \
    --enable-shared --with-hwloc=embedded  --with-ucx=embedded --with-libfabric=embedded --enable-fortran=all --with-ch4-shmmods=posix \
   CC=gcc F77=gfortran FC=gfortran CXX=g++ \
   MPICHLIB_CPPFLAGS="-I$HOME/misc.libs/lustre-release/lustre/include -I$HOME/misc.libs/lustre-release/lustre/include/uapi" \
   MPICHLIB_CFLAGS='-Wno-unused-but-set-variable -fno-omit-frame-pointer' \
   MPICHLIB_FCFLAGS='-fno-omit-frame-pointer' \
   MPICHLIB_CXXFLAGS='-Wno-unused-but-set-variable -fno-omit-frame-pointer' \
   2>&1 | tee configure.log


#   CPPFLAGS="-I$WORKINIT/misc.libs/lustre-release/lustre/include -I$WORKINIT/misc.libs/lustre-release/lustre/include/uapi -I$HOMEINIT/openshmem/include" \
#   LDFLAGS="-L$HOMEINIT/openshmem/lib64 -L$HOMEINIT/openpmix/lib64" \

#--with-pic=yes
#    --with-ucx=$HOMEINIT/ucx   \
#    --with-ib-include=/global/panfs01/users/aknyaze1/ofed/mlnx/include --with-ib-libpath=/usr/local/ofed/CURRENT/lib64  \

exit_code=`tail -n 1 config.log | grep "exit 0" `
echo configure done with $exit_code
sleep 10
if [ -n "$exit_code" ] ; then
make clean
make -j 16 2>&1 | tee make.log
make install  2>&1 | tee -a make.log
fi

)


