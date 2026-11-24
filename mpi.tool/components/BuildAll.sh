#!/bin/bash

find -name "*.o" -exec rm {} \; #clean all objects

pushd ctt/src
./Build.sh 
popd 

pushd mat/src
./BuildAll.sh
popd 

pushd pet/src
./Build.sh 
popd 

pushd tractor 
for c in intel gnu nvidia clang ; do ./BuildAll.sh $c ; done
popd 

pushd urpu/src 
./BuildAll.sh 
popd 

pushd qTV
./build.sh
popd

