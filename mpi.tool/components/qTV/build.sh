#!/bin/bash

#. /opt/intel/compiler/2015.1.133/bin/compilervars.sh intel64
#. /opt/intel/impi/5.1.2.150/impi/5.1.2.150/bin64/mpivars.sh

MYQT=/opt/Qt/6.11.2/gcc_64
export PATH=${MYQT}/bin:$PATH
#export QMAKESPEC=${MYQT}/mkspecs/linux-g++
rm -rf build.cl
mkdir -p build.cl

#. env.sh 

export CC=gcc
export CXX=g++

cd build.cl
    cmake \
        -G "Unix Makefiles" \
        -DCMAKE_INSTALL_PREFIX=../../.. \
        ../
    cmake --build . -- -j${threads}
    cmake --build . -- install

