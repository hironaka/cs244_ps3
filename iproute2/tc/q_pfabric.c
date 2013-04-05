/*
 * q_pfabric	pFabric
 *
 * Authors: Yan Michalevsky <yanm2@stanford.edu>
 *			Hannah Hironaka <hannahhironaka@stanford.edu>
 *
 */

#include <stdio.h>
#include "utils.h"
#include "tc_util.h"
#include "tc_common.h"

#ifdef DEBUG
#define TRACE(x) x
#else
#define TRACE(x)
#endif

struct tc_pfabric_qopt {
	__u32 limit;
	int disable_dequeue;
};

#define DEFAULT_PACKET_BUFFER_LIMIT (150)

static void explain(void)
{
	fprintf(stderr,
"Usage: ... pfabric [ limit PACKETS ] \n"
"					[ disable_dequeue ] \n"
"					[ enable_dequeue ] \n"
);
}

static void explain1(const char *arg)
{
	fprintf(stderr, "Illegal \"%s\"\n", arg);
}

static int pfabric_parse_opt(struct qdisc_util *qu, int argc,
							 char** argv, struct nlmsghdr* n)
{
	struct rtattr* tail = NULL;
	struct tc_pfabric_qopt opt = 
		{ .limit = DEFAULT_PACKET_BUFFER_LIMIT, .disable_dequeue = 0 };
	
	TRACE( printf("pfabric_parse_opt called\n") );	
	
	for ( ; argc > 0; --argc, ++ argv ) {
		if (matches(*argv, "limit") == 0) {
			NEXT_ARG();
			if (get_size(&opt.limit, *argv)) {
				explain1("limit");
				return -1;
			}
		}
		else if (matches(*argv, "disable_dequeue") == 0) {
			NEXT_ARG();
			opt.disable_dequeue = 1;
		}
		else if (matches(*argv, "enable_dequeue") == 0) {
			NEXT_ARG();
			opt.disable_dequeue = 0;
		}
		else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
	}
		
	if (addattr_l(n, 1024, TCA_OPTIONS, &opt, sizeof(opt)) < 0) {
		fprintf(stderr, "q_pfabric: addattr_l failed\n");
		return -1;
	}

	tail = NLMSG_TAIL(n);
	tail->rta_len = (void*) NLMSG_TAIL(n) - (void*) tail;
	return 0;
}

static int pfabric_print_opt(struct qdisc_util *qu, FILE* f, struct rtattr* opt)
{
	struct tc_pfabric_qopt qopt;
	int len = RTA_PAYLOAD(opt) - sizeof(qopt);
	
	TRACE( fprintf(stderr, "pfabric_print_opt called\n") );
	
	if (NULL == opt) {
		return 0;
	}
	if (len < 0) {
		fprintf(stderr, "options size error\n");
		return -1;
	}

	memcpy(&qopt, RTA_DATA(opt), sizeof(qopt));
	fprintf(f, "limit %d", qopt.limit);
	fprintf(f, "disable_dequeue %d", qopt.disable_dequeue);
	return 0;
}

struct qdisc_util pfabric_qdisc_util = {
	.id			= "pfabric",
	.parse_qopt	= pfabric_parse_opt,
	.print_qopt	= pfabric_print_opt,
};
