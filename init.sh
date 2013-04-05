#!/bin/bash

# Install prerequisites for iproute2
sudo apt-get -y install flex bison libdb-dev

git submodule init
git submodule update
cd iproute2
./configure
make
cd ..

./build-patched-iperf.sh

cd src/kernel
make
cd ../../
