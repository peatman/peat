#!/bin/bash

compiler=oneapi:2026.1.0
. env.sh

#assuming gcc is installed 

make clean
make COMPILER=$1 install 
