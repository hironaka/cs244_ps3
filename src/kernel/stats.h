#ifndef __STATS_H__
#define __STATS_H__

#define PFAB_STATS_PROC_NAME "pfabric_stats_%s"
#define PFAB_CSVSTATS_PROC_NAME "pfabric_stats_csv_%s"
#define MAX_PROC_NAME_LEN (255)

int pfab_stats_init(const char* suffix);
void pfab_stats_exit(const char* suffix);

#endif
