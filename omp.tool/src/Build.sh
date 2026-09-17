#!/bin/bash

compiler=oneapi:latest
. env.sh

#assuming gcc is installed 

make clean
make COMPILER=$1 install 
