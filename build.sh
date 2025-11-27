#!/bin/bash

mkdir out
cd out
cmake -DCMAKE_BUILD_TYPE=Debug ../
make -j 32
./containerdebug
