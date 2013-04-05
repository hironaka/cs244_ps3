#!/bin/bash

set -e
set -o nounset

ctrlc() {
	echo "Trapped CTRL-C"
	killall -9 python
	mn -c
	rmmod pfabric
	exit
}

trap ctrlc SIGINT

start=`date`
exptid=`date +%b%d-%H:%M`
rootdir=pfabric-$exptid
iperf=~/iperf-patched-pa3/src/iperf
tc=CUSTOM_TC_PATH
n_hosts=2
n_nodes=`expr $n_hosts + 1`
total_bw=40
bw=`expr $total_bw / $n_hosts`
n_runs=1
max_q_default=150
max_q_pfabric=15
pfabric_ko_name=pfabric
pfabric_ko=src/kernel/${pfabric_ko_name}.ko
pythondir=src/python
evaluation_script=eval_utils.py
output_plot=evaluation

# Inject pFabric kernel module
module_loaded=`lsmod | grep $pfabric_ko_name | wc -l`
if [ $module_loaded == 0 ] 
then
    insmod $pfabric_ko
fi

for n_flows in 1 2 3 4 8 10 12 15 20; do #24 28 32 36 40; do
	
	python $pythondir/pfabric.py --bw $bw \
		                     --dir $rootdir \
		                     --nflows $n_flows \
		                     -n $n_nodes \
			             --iperf $iperf \
			             --maxq $max_q_default \
                                     --maxq_pfab $max_q_pfabric \
                                     --nruns $n_runs
done

rmmod $pfabric_ko_name

# Create plot
echo "python ${pythondir}/${evaluation_script} $rootdir $n_hosts $n_runs ${rootdir}/${output_plot}"
python ${pythondir}/${evaluation_script} $rootdir $n_hosts $n_runs ${rootdir}/${output_plot}

echo "Started at" $start
echo "Ended at" `date`
