#!/usr/bin/env python
from mininet.topo import Topo
from mininet.node import CPULimitedHost
from mininet.link import TCLink
from mininet.net import Mininet
from mininet.log import lg
from mininet.util import dumpNodeConnections

import subprocess
from subprocess import Popen, PIPE
import sys
import os
from argparse import ArgumentParser
from time import sleep

import pdb

CUSTOM_TC_PATH = 'iproute2/tc/tc'
CUSTOM_IPERF_PATH = '~/iperf-patched-pa3/src/iperf'
PROC_FILE_PATH = '/proc/pfabric_stats'

IPERF_PORT = 5001
IPERF_SEARCH_OUTPUT = 'iperf_search_h%d_p%d_f%d.txt'
IPERF_BASELINE_OUTPUT = 'iperf_baseline_%d.txt'
IPERF_CLIENT_COMMAND = '%s -c %s -p %d -n %d -S %d > %s &'
IPERF_SERVER_COMMAND = '%s -s -p %d > %s/iperf_server.txt &'

DELETE_QDISC = 'sudo %s qdisc del dev %s root'
HTB_ADD = 'sudo %s qdisc add dev %s root handle 1: htb'
HTB_CLASS_ADD = 'sudo %s class add dev %s parent 1: classid 1:1 htb rate %dmbit'
PFABRIC_ADD = 'sudo %s qdisc add dev %s parent 1:1 handle 10: pfabric limit %d'
PFIFO_ADD = 'sudo %s qdisc add dev %s parent 1:1 handle 10: pfifo limit %d'
DCTCP_SET = 'sysctl -w net.ipv4.tcp_dctcp_enable=%d'
ECN_SET = 'sysctl -w net.ipv4.tcp_ecn=%d'

MAX_QUEUE_DEFAULT = 150
MAX_QUEUE_PFAB_DEFAULT = 15
N_HOSTS_DEFAULT = 3
BW_DEFAULT = 10
DIR_DEFAULT = "results"
RUNS_DEFAULT = 5

SEARCH_FLOW_SIZE_BUCKETS = \
	[10**6.5, 10**6.6, 10**6.7, 10**6.8, 10**6.9, 10**7, 10**7.1, 10**7.2]

NUM_FLOW_SIZE_BUCKETS = len(SEARCH_FLOW_SIZE_BUCKETS)

"""Parse Arguments"""
parser = ArgumentParser(description="pFabric tests")
parser.add_argument('--bw', '-B',
                    dest="bw",
                    type=float,
                    action="store",
                    help="Bandwidth of links",
                    default=BW_DEFAULT)

parser.add_argument('--dir', '-d',
                    dest="dir",
                    action="store",
                    help="Directory to store outputs",
                    default=DIR_DEFAULT)

parser.add_argument('-n',
                    dest="n",
                    type=int,
                    action="store",
                    help="Number of nodes in star.  Must be >= 3",
                    default=N_HOSTS_DEFAULT)

parser.add_argument('--maxq',
                    dest="maxq",
                    action="store",
                    type=int,
                    help="Max buffer size of network interface in packets",
                    default=MAX_QUEUE_DEFAULT)

parser.add_argument('--maxq_pfab',
                    dest="maxq_pfab",
                    action="store",
                    type=int,
                    help="Max buffer size of pfabric network interface in packets",
                    default=MAX_QUEUE_PFAB_DEFAULT)

parser.add_argument('--iperf',
                    dest="iperf",
                    help="Path to custom iperf",
                    default=CUSTOM_IPERF_PATH)

parser.add_argument('--tc',
                    dest="tc",
                    help="Path to custom tc",
                    default=CUSTOM_TC_PATH)

parser.add_argument('--nflows',
                    dest="nflows",
                    action="store",
                    type=int,
                    help="Number of flows per host (for TCP)",
                    required=True)

parser.add_argument('--nruns',
                    dest="nruns",
                    action="store",
                    type=int,
                    help="Number of runs of the experiment",
                    default=RUNS_DEFAULT)

"""Experiment paramenters"""
args = parser.parse_args()

"""Create output directory"""
if not os.path.exists(args.dir):
    os.makedirs(args.dir)

lg.setLogLevel('info')

class BaseTest():
    """Base class for all our tests."""

    def __init__(self, name, tc=args.tc, 
                 bw=args.bw, iperf=args.iperf,
                 limit=args.maxq, limit_pfab=args.maxq_pfab, 
                 nflows=args.nflows, 
                 r=args.nruns, n=args.n):

        self.name = name
        self.tc = tc
        self.bw = bw
        self.limit = limit
        self.limit_pfab = limit_pfab
        self.nflows = nflows
        self.nruns = r
        self.n = n
        self.iperf = iperf
        self.server = 'h%d' % (self.n - 1)
        self.output_dir = '%s/%s' % (args.dir, self.name)
        self.interfaces = []
        for i in range(1, self.n + 1):
             self.interfaces.append('s0-eth%d' % (i))

        if not os.path.exists(self.output_dir):
                os.makedirs(self.output_dir)

    def tear_down(self):
        """Removes qdiscs, kill iperf processes."""
        print "Tear down %s..." % self.name
        os.system('killall -2 ' + self.iperf)

    def set_up(self):
        """Removes qdiscs, 
           adds HTB qdisc as root with specified bandwidth.
           adds PFIFO qdisc to limit max buffer size."""
        print "Set up %s..." % self.name
        for iface in self.interfaces:
            cmd = DELETE_QDISC % (self.tc, iface)
	    print cmd
            os.system(cmd)
            cmd = HTB_ADD % (self.tc, iface)
	    print cmd
            os.system(cmd)
            cmd = HTB_CLASS_ADD % (self.tc, iface, self.bw)
            print cmd
            os.system(cmd)
        
        cmd = PFIFO_ADD % (self.tc, 's0-eth%d' % self.n, self.limit)
        print cmd
        os.system(cmd)

    def test(self, net):
        """Start nflows on host 1 through n-1.
           Flow size distribution is similar to those 
           measured in production web search data centers. """
        self.set_up()
        self.start_receiver(net)
        self.start_senders(net)
        self.tear_down()

    def iperf_client_cmd(self, server_ip, n_bytes, priority, output_path):
        """Creates the iperf client command string given server ip, 
           n_bytes, priority, and output_path."""

        if self.name == 'tcp-droptail':
            priority = 0
        
        iperf_args = (self.iperf, 
                      server_ip, 
                      IPERF_PORT, 
                      n_bytes,
                      priority, 
                      output_path)

        cmd = IPERF_CLIENT_COMMAND % iperf_args

        print cmd
        return cmd

    def start_receiver(self, net):
        """Start iperf server."""
        print "Starting iperf server..."
        server = net.getNodeByName(self.server)
        cmd = IPERF_SERVER_COMMAND % (self.iperf, IPERF_PORT, self.output_dir)
        print cmd
        server.cmd(cmd)
        
    def start_senders(self, net):
        """Start iperf clients."""
        print "Starting %s web search traffic..." % self.name
        server = net.getNodeByName(self.server)
        clients = {}
        for r in range(self.nruns):
            output_dir = '%s/f%s-r%d' % (self.output_dir, self.nflows, r)
            if not os.path.exists(output_dir):
                os.makedirs(output_dir)

            for i in range(self.n - 1):
    	        client = net.getNodeByName('h%d' % i)
                pids = []
	        for j in range(self.nflows):
                    bucket = j % NUM_FLOW_SIZE_BUCKETS
                    n_bytes = SEARCH_FLOW_SIZE_BUCKETS[bucket]
                    output_file = IPERF_SEARCH_OUTPUT % (i, bucket, j)
                    output_path = '%s/%s' % (output_dir, output_file)
                    cmd = self.iperf_client_cmd(server.IP(), n_bytes, bucket * 4, output_path)
                    client.cmd(cmd)
                    pid = int(client.cmd('echo $!'))     
                    pids.append(pid)   
                    clients[client] = pids
    
            for h, pids in clients.items():
                for pid in pids:
                    h.cmd('wait', pid)

class BaselineTest(BaseTest):
    """Baseline test to measure best possible completion times."""

    def __init__(self):
        BaseTest.__init__(self, 'baseline')

    def start_senders(self, net):
        """Start one flow of each size at a time,
           used to measure ideal flow completion times."""
        print "Starting baseline traffic..."
        for r in range(self.nruns):
            output_dir = '%s/f%s-r%d' % (self.output_dir, self.nflows, r)
            if not os.path.exists(output_dir):
                os.makedirs(output_dir)
        
            server = net.getNodeByName(self.server)
            client = net.getNodeByName('h0')
            for i in range(NUM_FLOW_SIZE_BUCKETS):
                output_file = IPERF_BASELINE_OUTPUT % i
                output_path = '%s/%s' % (output_dir, output_file)
                bucket = i % NUM_FLOW_SIZE_BUCKETS
                n_bytes = SEARCH_FLOW_SIZE_BUCKETS[bucket]
                cmd = self.iperf_client_cmd(server.IP(), n_bytes, bucket * 4, output_path)
                client.cmd(cmd)
                pid = int(client.cmd('echo $!'))     
                client.cmd('wait', pid)
    
class pFabricTest(BaseTest):
    """pFabric test class"""

    def __init__(self):
        BaseTest.__init__(self, 'pfabric')

    def set_up(self):
        """Set to HTB with pFabric qdisc."""
        print "Set up pFabric..."
        for iface in self.interfaces:
            cmd = DELETE_QDISC % (self.tc, iface)
	    print cmd
            os.system(cmd)
            cmd = HTB_ADD % (self.tc, iface)
	    print cmd
            os.system(cmd)
            cmd = HTB_CLASS_ADD % (self.tc, iface, self.bw)
            print cmd
            os.system(cmd)
        
        cmd = PFABRIC_ADD % (self.tc, 's0-eth%d' % self.n, self.limit_pfab)
        print cmd
        os.system(cmd)

    def print_stats():
        """Prints pFabric statistics from proc file, and logs."""
        print "pFabric Statistics"
        f = open(PROC_FILE_PATH, 'r')
        print f.read()

	def tear_down(self):
		os.system('cat /proc/pfabric_stats_s0-eth%d' % self.n)
		BaseTest.tear_down(self)

class TCPDropTailTest(BaseTest):
    """TCPDropTail test class"""

    def __init__(self):
        BaseTest.__init__(self, 'tcp-droptail')

class DCTCPTest(BaseTest):
    """DCTCP test class"""

    def __init__(self):
        BaseTest.__init__(self, 'dctcp')

    def set_up(self):
        """Enable DCTCP."""
        BaseTest.set_up(self)
        os.system(DCTCP_SET % 1)
        os.system(ECN_SET % 1)

    def tear_down(self):
        """Disable DCTCP."""
        BaseTest.tear_down(self)
        os.system(DCTCP_SET % 0)
        os.system(ECN_SET % 0)        

class StarTopo(Topo):
    """Star topology for Buffer Sizing experiment"""

    def __init__(self, n=3, bw=None, maxq=None):
        super(StarTopo, self ).__init__()
        self.n = n
        self.bw = bw
        self.maxq = maxq
        self.create_topology()

    def create_topology(self):
        """Create star topology"""
        hosts = []
        for i in range(self.n):
            host = self.addHost('h%d' % i)
            hosts.append(host)

        switch = self.addSwitch('s0')
        linkopts = dict()
	linkopts['bw'] = self.bw
        linkopts['max_queue_size'] = self.maxq
        for i in range(self.n):
        	self.addLink(hosts[i], switch, **linkopts)

def main():
    """Create network and run pFabric experiment"""
    topo = StarTopo(n=args.n, bw=args.bw, maxq=args.maxq)
    net = Mininet(topo=topo, host=CPULimitedHost, link=TCLink)
    net.start()
    dumpNodeConnections(net.hosts)
    net.pingAll()
    baseline = BaselineTest()
    baseline.test(net)
    droptail_tcp = TCPDropTailTest()
    droptail_tcp.test(net)
    dctcp = DCTCPTest()
    dctcp.test(net)
    pfabric = pFabricTest()
    pfabric.test(net)
    net.stop()
    Popen("killall -9 top bwm-ng tcpdump cat mnexec", shell=True).wait()

if __name__ == '__main__':
    try:
        main()
    except:
        print "-"*80
        print "Caught exception.  Cleaning up..."
        print "-"*80
        import traceback
        traceback.print_exc()
        os.system("killall -9 top bwm-ng tcpdump cat mnexec iperf; mn -c")

