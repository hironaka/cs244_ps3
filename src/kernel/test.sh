#!/bin/bash
#NET_DEVICE_NAME=lo:1
#sudo ifconfig $NET_DEVICE_NAME 127.0.0.3
sudo dmesg -c > /dev/null
sudo insmod pfabric.ko
sudo dmesg
#./add_pfab.sh
#./del_pfab.sh
#sudo ifconfig $NET_DEVICE_NAME down
