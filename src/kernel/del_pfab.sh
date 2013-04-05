#!/bin/bash
TC=../../iproute2/tc/tc
IFC=lo
sudo $TC qdisc del dev $IFC root
sudo rmmod pfabric
lsmod | grep pfabric
