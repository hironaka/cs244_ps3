#!/usr/bin/env python

# An "Ideal" flow scheduler.
# Schedules flows in a non-decreasing manner according to size

import sys
import os
import numpy

ETHER_FRAME_LEN = 1500.0	# standard MTU
LINK_CAPACITY = 100e6 / 8.0	# 100Mb/s

class Flow:
	def __init__(self, size, start_time = 0):
		self.total_size = size
		self.remaining_size = size
		self.start_time = start_time
		self.completion_time = -1

	def get_remaining_size(self):
		return self.remaining_size

	def get_completion_time(self):
		return self.completion_time

	def is_done(self):
		return self.remaining_size <= 0

	def packet_sent(self, time):
		frame_overhead = 34 # Ethernet + IP header
		self.remaining_size -= min(ETHER_FRAME_LEN - frame_overhead, self.remaining_size)
		if self.is_done():
			self.completion_time = time - self.start_time
		
def get_file_size(filename):
	statinfo = os.stat(filename)
	return statinfo.st_size

def get_min_index(values):
	return numpy.array(values).argmin()

DELTA_T = float(ETHER_FRAME_LEN) / float(LINK_CAPACITY)

def schedule_flows(flow_sizes):
	flows = [Flow(flow_size) for flow_size in flow_sizes]
	flows_to_schedule = []
	flows_to_schedule.extend(flows)
	time = 0.0
	print 'Time quantum: %f' % DELTA_T
	
	while len(flows_to_schedule) > 0:
		time += DELTA_T
		remaining_sizes = [f.get_remaining_size() for f in flows_to_schedule]
		min_rs_flow = get_min_index(remaining_sizes)
		flows_to_schedule[min_rs_flow].packet_sent(time)
		flows_to_schedule = [f for f in flows_to_schedule if not f.is_done()]
	
	print 'All flows completed'
	for i in xrange(len(flows)):
		print 'Flow %d completion time: %f' % (i, flows[i].get_completion_time())

def main():
	flow_sizes = [int(s) for s in sys.argv[1:]]
	schedule_flows(flow_sizes)

if __name__ == '__main__':
	main()
