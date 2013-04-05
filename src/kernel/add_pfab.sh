#!/bin/bash
TC=../../iproute2/tc/tc
IFC=lo
sudo insmod pfabric.ko
lsmod | grep pfabric
$TC qdisc show
sudo $TC qdisc add dev $IFC root pfabric
$TC qdisc show
