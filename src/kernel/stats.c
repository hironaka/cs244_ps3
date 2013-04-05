#include <linux/version.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include "sch_pfab.h"
#include "stats.h"

static void *pfab_stats_proc_seq_start(struct seq_file *s, loff_t *pos)
{
	static unsigned long counter = 0;
	if (*pos == 0) {
		return &counter;
	}
	else {
		*pos = 0;
		return NULL;
	}
}

static void *pfab_stats_proc_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	unsigned long *index = (unsigned long *)v;
	(*index)++;

	return NULL;
}

static void pfab_stats_proc_seq_stop(struct seq_file *s, void *v)
{
	/* nothing to do, we use a static value in start() */
}

static int pfab_stats_proc_seq_show(struct seq_file *s, void *v)
{
	int i;
		
	TRACE( printk("pfab_stats_proc_seq_show called\n") );

	/* TODO: support more than one p_fab context. */
	seq_printf(s, 
		   "limit: %u\ndisable_dequeue: %d\ndropped:%u\npackets:%u\nbytes:%llu\n"
		   "Non-IP packets: %u\nillegal priority occurances: %u\n",
		   pfab_stats.limit,
		   pfab_stats.disable_dequeue,
		   pfab_stats.tc_stats.drops, 
		   pfab_stats.tc_stats.packets, 
		   pfab_stats.tc_stats.bytes,
		   pfab_stats.non_ip_packet_counter,
		   pfab_stats.illegal_prio_occurance);

	seq_printf(s, "bitmap:\n");
	for(i = 0; i < BITMAP_SIZE; i++) {
		seq_printf(s, "%08x\n", pfab_stats.bitmap[i]);
	}

	seq_printf(s, "Packet distribution across bands:\n");
	for (i = 0; i < NUM_BANDS; ++i) {
		seq_printf(s, "Band %u: %u\n", i, pfab_stats.per_band_packet_counter[i]);
	}

	return 0;
}

static int pfab_csvstats_proc_seq_show(struct seq_file *s, void *v) {
	
	int i;

	TRACE( printk("pfab_csvstats_proc_seq_show called\n") );

	/* TODO: support more than one p_fab context. */
	seq_printf(s, 
		   "limit,\tdropped,\tenqueues,\tdequeues,\tnon-ip packets,\tillegal priority\n"
		   "%u,\t%u,\t\t%u,\t\t%llu,\t\t%u\t\t%u\n",
		   pfab_stats.limit, 
		   pfab_stats.tc_stats.drops, 
		   pfab_stats.tc_stats.packets, 
		   pfab_stats.tc_stats.bytes,
		   pfab_stats.non_ip_packet_counter,
		   pfab_stats.illegal_prio_occurance);

	seq_printf(s, "bitmap\n");
	for(i = 0; i < BITMAP_SIZE; i++) {
		seq_printf(s, "%08x\n", pfab_stats.bitmap[i]);
	}

	return 0;
}

static struct proc_dir_entry *pfab_stats_proc = NULL;

static struct seq_operations pfab_stats_proc_seq_ops = {
	.start = pfab_stats_proc_seq_start,
	.next = pfab_stats_proc_seq_next,
	.stop = pfab_stats_proc_seq_stop,
	.show = pfab_stats_proc_seq_show
};

static int pfab_stats_proc_open(struct inode *inode, struct file *file)
{
	TRACE( printk("pfab_stats_proc_open called\n") );
	return seq_open(file, &pfab_stats_proc_seq_ops);
}

static struct file_operations pfab_stats_proc_file_ops = {
	.owner = THIS_MODULE,
	.open = pfab_stats_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};


/* For program friendly CSV stats */
static struct proc_dir_entry *pfab_csvstats_proc;

static struct seq_operations pfab_csvstats_proc_seq_ops = {
	.start = pfab_stats_proc_seq_start,
	.next = pfab_stats_proc_seq_next,
	.stop = pfab_stats_proc_seq_stop,
	.show = pfab_csvstats_proc_seq_show
};

static int pfab_csvstats_proc_open(struct inode *inode, struct file *file)
{
	TRACE( printk("pfab_csvstats_proc_open called\n") );
	return seq_open(file, &pfab_csvstats_proc_seq_ops);
}

static struct file_operations pfab_csvstats_proc_file_ops = {
	.owner = THIS_MODULE,
	.open = pfab_csvstats_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};


int pfab_stats_init(const char* suffix) {
	int ret = 0;
	char proc_name[MAX_PROC_NAME_LEN + 1] = {0};

	TRACE( pr_debug("pfab_stats_init (%s)\n", suffix) );
	
	snprintf(proc_name, MAX_PROC_NAME_LEN, PFAB_STATS_PROC_NAME, suffix);
	pfab_stats_proc = create_proc_entry(proc_name, 0, NULL);
	if(pfab_stats_proc) {
		pfab_stats_proc->proc_fops = &pfab_stats_proc_file_ops;
	} else {
		TRACE( printk("pfab_stats_proc = NULL. Failed initializing stats\n") );
		ret = 1;
		goto out;
	}

	snprintf(proc_name, MAX_PROC_NAME_LEN, PFAB_CSVSTATS_PROC_NAME, suffix);
	pfab_csvstats_proc = create_proc_entry(proc_name, 0, NULL);
	if(pfab_csvstats_proc) {
		pfab_csvstats_proc->proc_fops = &pfab_csvstats_proc_file_ops;
	} else {
		TRACE( printk("pfab_csvstats_proc = NULL. Failed initializing stats\n") );
		ret = 1;
		remove_proc_entry(proc_name, NULL);
		goto out;
	}

	TRACE( printk("Initilized stats\n") );

 out:
	return ret;
}

void pfab_stats_exit(const char* suffix) {
	char proc_name[MAX_PROC_NAME_LEN + 1] = {0};

	TRACE( printk("pfab_stats_exit called\n") );

	snprintf(proc_name, MAX_PROC_NAME_LEN, PFAB_STATS_PROC_NAME, suffix);
	remove_proc_entry(proc_name, NULL);

	snprintf(proc_name, MAX_PROC_NAME_LEN, PFAB_CSVSTATS_PROC_NAME, suffix);
	remove_proc_entry(proc_name, NULL);
}
