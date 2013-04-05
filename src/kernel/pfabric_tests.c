#include <linux/ip.h>
#include "sch_pfab.h"

#define NET_DEVICE_NAME "lo"

#define TEXT_RED "[0;31m"
#define TEXT_NORMAL "[0;37m"

#define ALLOC_SKB(skb, priority) do {	\
	skb = alloc_ip_skb(priority);		\
	if (NULL == skb) {					\
		return -1;						\
	}									\
} while (0)

static struct Qdisc* sch = NULL;

#define ETH_HEADER_LEN (14)
#define IP_HEADER_LEN (20)

static struct sk_buff* alloc_ip_skb(__u8 priority)
{
	struct sk_buff* skb = alloc_skb(ETH_HEADER_LEN + IP_HEADER_LEN, GFP_KERNEL);
	struct iphdr* ip_header = NULL;

	if (NULL == skb) {
		pr_err("Failed allocating skb\n");
		return NULL;
	}
	
	skb->mac_header = (sk_buff_data_t) skb_put(skb, ETH_HEADER_LEN);
	if (NULL == skb->mac_header) {
		pr_err("Failed putting MAC header data in skb\n");
		kfree_skb(skb);
		return NULL;
	}

	ip_header = (struct iphdr*) skb_put(skb, IP_HEADER_LEN);
	if (NULL == ip_header) {
		pr_err("IP header is NULL\n");
		kfree_skb(skb);
		return NULL;
	}
	skb->network_header = (sk_buff_data_t) ip_header;

	ip_header->version = 4; /* IPv4 packet */
	ip_header->tos = priority;						
	pr_debug("Allocated skb. Length %d\n", qdisc_pkt_len(skb));
	return skb;
}

static void disable_dequeue( int status )
{
	pfab_sched_data_t* pfab_data = qdisc_priv(sch);
	pfab_data->disable_dequeue = status;
	pr_info("Dequeuing %s\n", status ? "disabled" : "enabled");
}

static void set_limit( __u32 limit )
{
	pfab_sched_data_t* pfab_data = qdisc_priv(sch);
	pfab_data->limit = limit;
	pr_info("Limit set to %u\n", limit);
}

int setup( void )
{
	struct net_device* netdev = NULL;
	struct netdev_queue* netdev_queue = NULL;

	netdev = dev_get_by_name(&init_net, NET_DEVICE_NAME);
	if (NULL == netdev) {
		pr_err("Failed getting network device\n");
		return -1;
	}
	
	netdev_queue = netdev_get_tx_queue(netdev, 0);
	if (NULL == netdev_queue) {
		pr_err("Failed getting network device queue\n");
		return -2;
	}

	sch = qdisc_create_dflt(netdev_queue, &pfab_qdisc_ops, 0);
	if (NULL == sch) {
		pr_err("Failed allocating qdisc\n");
		return -3;
	}
	
	return 0;
}

void teardown( void )
{
	qdisc_destroy(sch);
}

int enqueue_dequeue_test( void )
{
	int retval;
	struct sk_buff* dequeued_skb = NULL;
	struct sk_buff* skb = NULL;
	
	pr_info("enqueue_dequeue_test\n");

	ALLOC_SKB(skb, 0);

	retval = pfab_qdisc_ops.enqueue(skb, sch);
	if (NET_XMIT_SUCCESS != retval) {
		pr_err("enqueue test failed\n");
		kfree_skb(skb);
		return retval;
	}

	disable_dequeue(1);
	dequeued_skb = pfab_qdisc_ops.dequeue(sch);
	if (NULL != dequeued_skb) {
		pr_err("Unexpectedly dequeued packet\n");
		kfree_skb(dequeued_skb);
		return -3;
	}

	disable_dequeue(0);

	dequeued_skb = pfab_qdisc_ops.dequeue(sch);
	if (NULL == dequeued_skb) {
		pr_err("dequeue failed\n");
		return -2;
	}

	if (dequeued_skb != skb) {
		pr_err("Unexpected skb dequeued. Not the same as the one enqueued\n");
	}

	kfree_skb(dequeued_skb);
	return retval;
} /* end of enqueue_dequeue_test */


int drop_test( void )
{
	int retval;
	struct sk_buff* skb = NULL;

	pr_info("drop_test\n");

	set_limit(0);
	disable_dequeue(1);

	ALLOC_SKB(skb, 0);
	retval = pfab_qdisc_ops.enqueue(skb, sch);
	if (NET_XMIT_CN != retval) {
		pr_err("Packet not dropped as expected\n");
		return retval;
	}
	
	pr_debug("Packet dropped as expected\n");

	set_limit(1);
	ALLOC_SKB(skb, 1);
	retval = pfab_qdisc_ops.enqueue(skb, sch);
	if (NET_XMIT_SUCCESS != retval) {
		pr_err("Expected %d but got %d\n", NET_XMIT_SUCCESS, retval);
		return retval;
	}

	pr_debug("Packet successfully enqueued as expected\n");

	ALLOC_SKB(skb, 2);
	retval = pfab_qdisc_ops.enqueue(skb, sch);
	if (NET_XMIT_DROP != retval) {
		pr_err("Expected %d but got %d\n", NET_XMIT_DROP, retval);
		return retval;
	}

	pr_debug("Packet dropped as expected\n");

	ALLOC_SKB(skb, 0);
	retval = pfab_qdisc_ops.enqueue(skb, sch);
	if (NET_XMIT_CN != retval) {
		pr_err("Expected %d but got %d\n", NET_XMIT_CN, retval);
		return retval;
	}

	disable_dequeue(0);
	skb = pfab_qdisc_ops.dequeue(sch);
	if (NULL == skb) {
		pr_err("Failed dequeuing packet\n");
		return -2;
	}

	kfree_skb(skb);

	return 0;
} /* end of drop_test */

int run_tests( void )
{
	int retval;

	pr_info("Running pFabric tests...\n");

	retval = setup();
	if (retval < 0) {
		pr_err("pFabric tests setup failed. Not proceeding to running tests\n");
		return retval;
	}

	retval = enqueue_dequeue_test();
	if (retval < 0) {
		pr_err("enqueue_dequeue_test failed (%d)\n", retval);
		goto tests_teardown;
	}

	retval = drop_test();
	if (retval < 0) {
		pr_err("drop_test failed (%d)\n", retval);
	}

tests_teardown:
	teardown();
	pr_info("pFabric tests completed with status %d\n", retval);
	return retval;
} /* end of run_tests */
