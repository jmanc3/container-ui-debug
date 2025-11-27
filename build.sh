#!/bin/bash

mkdir out
set -e
cd out
cmake -DCMAKE_BUILD_TYPE=Release ../
make -j 32
./containerdebug
