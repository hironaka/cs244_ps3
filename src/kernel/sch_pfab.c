/* 
 * Module: pFabric classful queueing discipline. 
 *
 * Authors: Yan Michalevsky <yanm2@stanford.edu> 
 * 			Hannah Hironaka <hannahhironaka@stanford.edu>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <net/netlink.h> 
#include <linux/ip.h>
#include "sch_pfab.h"
#include "stats.h"


#define BAND_TO_BIT_AND_BMAP(band, bit, b_map) do {		\
	bit = band % BANDS_IN_BITMAP;						\
	b_map = band / BANDS_IN_BITMAP;			 			\
} while (0)

#ifdef PFABRIC_TESTS
#define STATIC
extern int run_tests( void );
#else
#define STATIC static
#endif

/* Function Prototypes. */
static unsigned int pfab_drop(struct Qdisc *sch);

/* TODO: In the future we will need a list of these. */
struct pfab_stat_data pfab_stats;

static inline struct sk_buff_head*
band2list(pfab_sched_data_t* pfab, int band)
{
	return pfab->queues + band;
}

/* Returns the highest priority band that is not empty, given pfab schedule data. */
//TODO: Optimize.
static int bitmap_high_prio(pfab_sched_data_t *pfab_data) 
{
	int i, j;
	u32 mask = 1;
	int high_prio = -1;
	
	BUG_ON(!pfab_data);

	for (i = 0; i < BITMAP_SIZE; i++) {
		if (0 == pfab_data->bitmap[i]) {
			continue;
		}
		
		for (j = 0; j < BANDS_IN_BITMAP; j++) {
			if (pfab_data->bitmap[i] & (mask << j)) {
				high_prio = i * BANDS_IN_BITMAP + j; /* i * 32 + j */
				TRACE( printk("Highest enqueued priority: %d\n", high_prio) );
				return high_prio;
			}
		}
	}	

	return -1;
} 

/* Returns the lowest priority band that is not empty, given pfab schedule data. */
//TODO: Optimize.
static int bitmap_low_prio(pfab_sched_data_t *pfab_data) 
{
	int i, j;
	u32 mask = 1;
	int low_prio = -1;

	BUG_ON(!pfab_data);
	
	for (i = BITMAP_SIZE - 1; i >= 0; i--) {
		if (0 == pfab_data->bitmap[i]) {
			continue;
		}
		
		for (j = BANDS_IN_BITMAP - 1; j >= 0; j--) {
			if (pfab_data->bitmap[i] & (mask << j)) {
				low_prio = i * BANDS_IN_BITMAP + j; /* i * 32 + j */
				TRACE( printk("Lowest enqueued priority: %d\n", low_prio) );
				return low_prio;
			}
		}
	}
	
	return -1;
} 

/* Updates the bitmap that stores which queues are currently occupied, when a 
   packet was added to a band. */
static inline void bitmap_add_band(pfab_sched_data_t *pfab_data, int band)
{
	int bit, b_map;

	BAND_TO_BIT_AND_BMAP(band, bit, b_map);
	pfab_data->bitmap[b_map] |= (1 << bit);
	pfab_stats.bitmap[b_map] = pfab_data->bitmap[b_map];
}

/* Updates the bitmap that stores which queues are currently occupied, when 
   the last packet is removed from a band. */
static inline void bitmap_remove_band(pfab_sched_data_t *pfab_data, u32 band)
{
	int bit, b_map;

	BAND_TO_BIT_AND_BMAP(band, bit, b_map);
	pfab_data->bitmap[b_map] &= ~(1 << bit);
	pfab_stats.bitmap[b_map] = pfab_data->bitmap[b_map];
}

#define ETH_HEADER_LEN (14)
#define IP_HEADER_LEN (20)

static inline int get_skb_priority(struct sk_buff* skb)
{
	if (skb->len < (ETH_HEADER_LEN + IP_HEADER_LEN)) {
		pr_debug("skb length is: %d\n", skb->len);
		return -1; /* not an IP packet */
	}

	return ip_hdr(skb)->tos;
}

STATIC int pfab_enqueue(struct sk_buff *skb, struct Qdisc *sch) 
{
	int band;
	pfab_sched_data_t *pfab_data = NULL;
	struct sk_buff_head *list = NULL;
	int len;

	TRACE( pr_debug("pfab_enqueue called\n") );
	BUG_ON(!skb);
	BUG_ON(!sch);

	pfab_data = qdisc_priv(sch);
	BUG_ON(!pfab_data);

	band = get_skb_priority(skb);
	if (unlikely(band < 0)) {
		TRACE( pr_debug("Not an IP packet\n") );
		band = 0; /* let all other types through */
		++pfab_stats.non_ip_packet_counter;
	}

	/* Packet seems to be ok, set its priority according to TOS */
	skb->priority = band;

	if (unlikely(skb->priority >= NUM_BANDS || skb->priority < 0)) {
		pr_warning("Illegal packet priority (%d). Discarding...\n",
				   skb->priority);
		++pfab_stats.illegal_prio_occurance;
		return qdisc_drop(skb, sch);
	}

	++pfab_stats.per_band_packet_counter[skb->priority];

	TRACE( pr_debug("Enqueuing packet with priority %d\n", skb->priority) );
	TRACE( pr_debug("Queue length = %u, limit = %u\n", 
					skb_queue_len(&sch->q), pfab_data->limit) );
	
	if ( unlikely(skb_queue_len(&sch->q) >= pfab_data->limit) ) {
		TRACE( pr_debug("pFabric buffer is full\n") );
		band = bitmap_low_prio(pfab_data);
		if ((0 <= band) && (band <= skb->priority))  {
			pr_debug("Packet has lower priority (%d) than any other "
					 "in buffer (%d). Dropping...\n", 
					 skb->priority, band);
			return qdisc_drop(skb, sch);
		}
	}

	/* Enqueue the packet. */
	band = skb->priority;
	bitmap_add_band(pfab_data, band);
	sch->q.qlen++;
	pfab_stats.tc_stats.bytes += skb->len;
	pfab_stats.tc_stats.packets++;

	list = band2list(pfab_data, band);
	__qdisc_enqueue_tail(skb, sch, list);

	TRACE(pr_debug("New queue len: %u\n", skb_queue_len(&sch->q)));

	/* Drop a packet if we have exceeded buffer limit. */
	if ( unlikely(skb_queue_len(&sch->q) > pfab_data->limit) ) {
		pr_debug("Buffer is full. Dropping packet...\n");
		len = pfab_drop(sch);
		if ( unlikely(len < 0) ) {
			pr_alert("ERROR: Failed dropping a packet from an overflown queue\n");
		}
		else {
			return NET_XMIT_CN;
		}
	}	

	return NET_XMIT_SUCCESS;
}

STATIC struct sk_buff *pfab_dequeue(struct Qdisc *sch) 
{
	pfab_sched_data_t *pfab_data = NULL;
	int band;
	struct sk_buff_head *list = NULL;
	struct sk_buff *skb = NULL;
	
	/* TRACE( printk("pfab_dequeue called\n") ); */
	BUG_ON(!sch);

	pfab_data = qdisc_priv(sch);

	if (unlikely(pfab_data->disable_dequeue)) {
		pr_debug("Dequeuing disabled. Dropping packet...\n");
		return NULL;
	}

	band = bitmap_high_prio(pfab_data);
	BUG_ON(band >= NUM_BANDS);

	/*TRACE( printk("Highest priority band is %d\n", band) );*/

	if (band < 0) {
		/* No packets, return */
		/* TRACE( printk("No packets to dequeue\n") ); */
		return NULL;
	}

	/* If there are queued packets, dequeue. */
	list = band2list(pfab_data, band);
	skb = __qdisc_dequeue_head(sch, list);
	if ( unlikely(NULL == skb) ) {
		pr_warning("ERROR: an skb is expected in band %d\n", band);
		return NULL;
	}

	sch->q.qlen--;
	if (skb_queue_empty(list)) {
		/*TRACE( printk("Band %d empty, removing from bitmap...\n", band) );*/
		bitmap_remove_band(pfab_data, band);
	}

	return skb;
}

STATIC struct sk_buff *pfab_peek(struct Qdisc *sch) 
{
	pfab_sched_data_t *pfab_data = NULL;
	struct sk_buff_head *list = NULL;
	int band;

	pfab_data = qdisc_priv(sch);
	band = bitmap_high_prio(pfab_data);

	if ( band < 0 ) {
		return NULL;
	}

	/* If there is a packet... */
	list = band2list(pfab_data, band);
	return skb_peek(list);
}

STATIC unsigned int pfab_drop(struct Qdisc *sch) 
{
	pfab_sched_data_t *pfab_data = NULL;
	struct sk_buff_head *list = NULL;
	int band;
	unsigned int len;

	TRACE( printk("pfab_drop called\n") );

	BUG_ON(!sch);

	pfab_data = qdisc_priv(sch);
	band = bitmap_low_prio(pfab_data);
	if (band < 0) {
		pr_alert("low priority band not found\n");
		return -1;
	}
	
	TRACE(pr_debug("Found low priority packets in band %u\n", band));
	
	list = band2list(pfab_data, band);
	len = __qdisc_queue_drop_head(sch, list);
	sch->q.qlen--;
	sch->qstats.drops++;
	pfab_stats.tc_stats.drops++;
	return len;
}

STATIC int pfab_change(struct Qdisc* sch, struct nlattr *opt)
{
	pfab_sched_data_t* pfab_data = NULL;
	tc_pfabric_qopt_t* qopt = NULL;
	BUG_ON(!sch);
	
	TRACE( printk("pfab_change called\n") );
	
	if (NULL == opt) {
		TRACE( printk("opt is NULL\n") );
		return -EINVAL;
	}
	
	if ( nla_len(opt) < sizeof(*qopt) ) {
		return -EINVAL;
	}
	
	pfab_data = qdisc_priv(sch);
	qopt = nla_data(opt);

	pr_info("Setting limit=%d, disable_dequeue=%d\n",
			qopt->limit, qopt->disable_dequeue);
	pfab_data->limit = qopt->limit;
	pfab_stats.limit = qopt->limit;
	pfab_data->disable_dequeue = qopt->disable_dequeue;
	pfab_stats.disable_dequeue = qopt->disable_dequeue;
	
	return 0;
}

STATIC int pfab_init(struct Qdisc *sch, struct nlattr *opt) 
{
	pfab_sched_data_t *pfab_data = NULL;
	int i;
	int retval;
	struct net_device* netdev = NULL;
  
	BUG_ON(!sch);
	
	TRACE( printk("pfab_init called\n") );

	pfab_data = qdisc_priv(sch);
	BUG_ON(!pfab_data);
	
	netdev = qdisc_dev(sch);
	BUG_ON(!netdev);
	
	/* Initialize priority queues. */
	for (i = 0; i < NUM_BANDS; i++) {
		skb_queue_head_init(band2list(pfab_data, i));
	}
	
	/* Initialize bitmap. */
	memset(pfab_data->bitmap, 0, sizeof(pfab_data->bitmap));
		
	memset(&pfab_stats, 0, sizeof(pfab_stats));

	pfab_data->limit = DEFAULT_LIMIT;
	pfab_data->disable_dequeue = 0;
	
	/* Initialize statistics for proc file. */
	pfab_stats.limit = DEFAULT_LIMIT;
	pfab_stats.disable_dequeue = 0;

	retval = opt ? pfab_change(sch, opt) : 0;
	if (retval < 0) {
		pr_err("pfab_change failed\n");
		return retval;
	}

	BUG_ON(!netdev->name);
	retval = pfab_stats_init(netdev->name);
	if (retval < 0) {
		pr_err("Failed initializing statistics for pFabric\n");
	}

	return retval;
}

STATIC void pfab_reset(struct Qdisc *sch) 
{
	int prio;
	pfab_sched_data_t *pfab_data = NULL;
	
	TRACE( printk("pfab_reset called\n") );

	BUG_ON(!sch);
	
	pfab_data = qdisc_priv(sch);

	for (prio = 0; prio < NUM_BANDS; prio++) {
		__qdisc_reset_queue(sch, band2list(pfab_data, prio));
	}

	sch->qstats.backlog = 0;
	sch->q.qlen = 0;

	memset(&pfab_stats, 0, sizeof(pfab_stats));

	/* Initialize statistics for proc file. */
	pfab_stats.limit = DEFAULT_LIMIT;
	pfab_stats.disable_dequeue = 0;
}

STATIC void pfab_destroy(struct Qdisc *sch) 
{
	int prio;
	pfab_sched_data_t *pfab_data = qdisc_priv(sch);
	struct net_device* netdev = NULL;
	
	TRACE( printk("pfab_destroy called. Removing pFabric qdisc...\n") );

	BUG_ON(!sch);
	netdev = qdisc_dev(sch);
	BUG_ON(!netdev);
	BUG_ON(!netdev->name);
	
	for (prio = 0; prio < NUM_BANDS; prio++) {
		skb_queue_purge(band2list(pfab_data, prio));
	}

	pfab_stats_exit(netdev->name);
}

#ifdef CONFIG_RTLENTLINK
STATIC int pfab_dump(struct Qdisc* sch, struct sk_buff* skb)
{
	pfab_sched_data_t* pfab_data = NULL;
	struct nlattr* nla = (struct nlattr*) skb_tail_pointer(skb);
	tc_pfabric_qopt_t qopt;	

	TRACE( printk("pfab_dump called\n") );
	
	BUG_ON(!sch);
	pfab_data = qdisc_priv(sch);
	
	qopt.limit = pfab_data->limit;
	qopt.disable_dequeue = pfab_data->disable_dequeue;
	if ( nla_put(skb, TCA_OPTIONS, sizeof(qopt), &qopt) ) {
		pr_err("nla_put failed\n");
		goto dump_error;
	}
		
	return nla_nest_end(skb, nla);

dump_error:
	nlmsg_trim(skb, nla);
	return -1;
}
#endif

STATIC struct Qdisc_ops pfab_qdisc_ops __read_mostly = {
	.next		=	NULL,
	.id			=	QDISC_ID,
	.priv_size	=	sizeof(pfab_sched_data_t),
	.enqueue	=	pfab_enqueue,
	.dequeue	=	pfab_dequeue,
	.peek		=	pfab_peek,
	.init		=	pfab_init,
	.change		=	pfab_change,
	.reset		=	pfab_reset,
	.destroy	=	pfab_destroy,
	.owner		=	THIS_MODULE,

#ifdef CONFIG_RTLENTLINK
	.dump		=	pfab_dump,
#endif
};

static int __init pfab_module_init(void)
{
	int retval;
	pr_info("Initializing pFabric...\n");

#ifdef PFABRIC_TESTS
	retval = run_tests();
	if (retval < 0) {
		pr_err("pFabric tests failed\n");
	}

	return -1;
#endif


	pr_info("Registering qdisc...\n");
	retval = register_qdisc(&pfab_qdisc_ops);
	if (retval < 0) {
		pr_err("Failed registering pFabric qdisc\n");
		return retval;
	}

	return 0;
}

static void __exit pfab_module_exit(void)
{
	pr_info("Unregistering qdisc...\n");
	unregister_qdisc(&pfab_qdisc_ops);
}

module_init(pfab_module_init);
module_exit(pfab_module_exit);

MODULE_LICENSE("GPL");

