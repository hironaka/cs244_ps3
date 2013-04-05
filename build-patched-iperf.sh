#!/bin/bash
PA3_DIR=`pwd`
IPERF_VER='iperf-2.0.5'

cd $HOME
wget http://downloads.sourceforge.net/project/iperf/iperf-2.0.5.tar.gz
tar -zxf ${IPERF_VER}.tar.gz
cd ${IPERF_VER}
#patch -p1 < $PA3_DIR/patches/iperf-Option-priority-allows-user-to-set-SO_PRIORITY.patch
cd src
patch -p1 < $PA3_DIR/patches/iperf-2.0.5-wait-syn.patch
cd ..
./configure
make CPPFLAGS=-DSO_PRIORITY
cd $HOME
rm ${IPERF_VER}.tar.gz
mv ${IPERF_VER} iperf-patched-pa3
cd $PA3_DIR
