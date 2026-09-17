#!/bin/sh
compiler=oneAPI:2026.1.0
. env.sh
make clean
make -j 16 2>&1 | tee Build.log
make install 2>&1 | tee -a Build.log