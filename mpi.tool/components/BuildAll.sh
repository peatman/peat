#!/bin/bash

find -name "*.o" -exec rm {} \; #clean all objects

pushd ctt/src
./Build.sh 
popd 

pushd mat/src
./BuildAll.sh
popd 

pushd tractor 
./BuildAlli.sh
./BuildAllg.sh
popd 

pushd qTV
./build.sh
popd

