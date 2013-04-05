#!/usr/bin/env python
import sys
import os
import numpy
import matplotlib
matplotlib.use('Agg')
import pylab
import plot_defaults

def get_fct_from_file(filename):
    print 'Reading file', filename
    lines = open(filename, 'r').readlines()
    fct = float(lines[-1].split('-')[1].split('sec')[0].strip())
    return fct

def nan_mean(a, axis):
    ma = numpy.ma.masked_array(a, numpy.isnan(a))
    return numpy.mean(ma, axis = axis)

NUM_BUCKETS = 8

def normalize_fcts(fcts, baseline):
    normalized_fcts = [fcts[i]/baseline[i%NUM_BUCKETS] for i in xrange(len(fcts))]
    return numpy.array(normalized_fcts)

def get_avg_normalized_fct(num_hosts, flows_per_host, baseline_template, search_template):
	"""
	Calculates the average FCT (Flow Completion Time) across all the flows on all
	the hosts for the current setup.
	"""
	# Extract baselines
	baselines = [get_fct_from_file(f) for f in [baseline_template % (flow %
	    NUM_BUCKETS) for flow in xrange(flows_per_host)]]
	print 'Baseline:', baselines

	host_fcts = [[get_fct_from_file(host_files) for host_files in \
		[search_template % (host, flow % NUM_BUCKETS, flow) for \
			flow in xrange(flows_per_host)]] for host in xrange(num_hosts)]
	print 'Host FCT matrix:', host_fcts
	average_per_flow = numpy.matrix(host_fcts).mean(0)
	print 'Average FCT per host:', average_per_flow
	normalized_fct_per_flow = normalize_fcts(average_per_flow, baselines)
	print 'Normalized FCT per flow', normalized_fct_per_flow
	mean_fct = normalized_fct_per_flow.mean()
	print 'Mean FCT:', mean_fct
	return mean_fct

TEMPLATE = os.path.join('%s', '%s', 'f%d-r%d', '%s')

def get_loads_list(base_dir):
	print base_dir
	d = [x for x in os.listdir(os.path.join(base_dir, 'baseline')) if \
		os.path.isdir(os.path.join(base_dir, 'baseline', x)) and
		x.startswith('f')]
	print d
	d = [int(x.split('f')[1].split('-r')[0]) for x in d]
	return list(set(d))

def evaluate_scheme(base_dir, scheme, num_iter, num_hosts, loads):
	print 'Evaluatingi scheme:', scheme
	fcts_all_iter = []
	for i in xrange(num_iter):
	    fcts = []
	    for load in loads:
		baseline_template = TEMPLATE % (base_dir, 'baseline', load, i, 
						'iperf_baseline_%d.txt')
		search_template = TEMPLATE % (base_dir, scheme, load, i, 
					      'iperf_search_h%d_p%d_f%d.txt')
		print baseline_template, search_template
		fcts.append( get_avg_normalized_fct(num_hosts, load, baseline_template, search_template) )
		fcts_all_iter.append(fcts)
	print 'Scheme data:', fcts_all_iter
	return numpy.matrix(fcts_all_iter).mean(0).tolist()[0]

def evaluate_all(base_dir, num_hosts, num_iter, schemes, plot_output):
	matplotlib.rc('figure', figsize=(16, 16))
	fig = pylab.figure()
	p = fig.add_subplot(111)
	loads = get_loads_list(base_dir)
	loads.sort()
	for scheme in schemes:
		fcts = evaluate_scheme(base_dir, scheme, num_iter, num_hosts, loads)
		print 'Loads:', loads, 'FCTS:', fcts
		p.plot(loads, fcts, label = scheme, lw=2)

	handles, labels = p.get_legend_handles_labels()
	p.legend(handles, labels)
	
	matplotlib.pyplot.xlabel('Number of flows per host')
	matplotlib.pyplot.ylabel('Average Normalized FCT')
	matplotlib.pyplot.grid(True)
	if plot_output is None:
	    print 'No output file specified, showing figure...'
	    matplotlib.pyplot.show()
	else:
	    print 'Saving output to %s' % plot_output
	    matplotlib.pyplot.savefig(plot_output)

	
SCHEMES = ['tcp-droptail', 'pfabric']

def main():
	"""
	Evaluates several setups with different schemes and loads.
	"""
	base_dir = sys.argv[1]
	num_hosts = int(sys.argv[2])
	num_iter = int(sys.argv[3])
	plot_output = None
	if len(sys.argv) > 4:
		plot_output = sys.argv[4]
	fcts_vector = evaluate_all(base_dir, num_hosts, num_iter, SCHEMES, plot_output)

if __name__ == '__main__':
	main()
