/* 
 * Module: pFabric classful queueing discipline. 
 *
 * Authors: Yan Michalevsky <yanm2@stanford.edu> 
 * 			Hannah Hironaka <hannahhironaka@stanford.edu>
 */

#ifndef __SCH_PFAB_H__
#define __SCH_PFAB_H__

#include <linux/list.h>
#include <net/pkt_sched.h>

//TODO: make these options.
#define BITS_IN_BYTE (8)
#define BANDS_IN_BITMAP (sizeof(u32) * BITS_IN_BYTE)

#define NUM_BANDS (32)
#define BITMAP_SIZE (NUM_BANDS / BANDS_IN_BITMAP)

/* Default packet buffer size */
#define DEFAULT_LIMIT (150)

#ifdef DEBUG
#define TRACE(x) x
#else
#define TRACE(x)
#endif

/* This should correspond to the identifier used by tc */
#define QDISC_ID "pfabric"

struct pfab_stat_data {
	u32 limit; 			//Limit of the queue size.
	int disable_dequeue;
	u32 bitmap[BITMAP_SIZE];	//Bitmap indicating occupied queues.
	struct tc_stats tc_stats;
	u32 per_band_packet_counter[NUM_BANDS];
	u32 non_ip_packet_counter;
	u32 illegal_prio_occurance;
};

/* Fields ordering should correspond to that in tc in order for
the Netlink messages to be parsed correctly */
struct tc_pfabric_qopt {
	/* Packet buffer size */
	__u32 limit;

	/* 
	   We use dequeue disabling for testing.
	   dequeuing is disabled to enable filling 
	   pFabric's packet buffer.
	 */
	int	disable_dequeue;
};

typedef struct tc_pfabric_qopt tc_pfabric_qopt_t;

extern struct pfab_stat_data pfab_stats;

typedef struct pfab_sched_data {
	u32 limit; 
	u32 bitmap[BITMAP_SIZE];
	struct sk_buff_head queues[NUM_BANDS];
	int disable_dequeue;
} pfab_sched_data_t;

#ifdef PFABRIC_TESTS
int pfab_enqueue(struct sk_buff *skb, struct Qdisc *sch);
struct sk_buff *pfab_dequeue(struct Qdisc *sch);
struct sk_buff *pfab_peek(struct Qdisc *sch);
int pfab_change(struct Qdisc* sch, struct nlattr *opt);
int pfab_init(struct Qdisc *sch, struct nlattr *opt);
void pfab_reset(struct Qdisc *sch);
void pfab_destroy(struct Qdisc *sch); 
int pfab_dump(struct Qdisc* sch, struct sk_buff* skb);

extern struct Qdisc_ops pfab_qdisc_ops;
#endif

#endif
