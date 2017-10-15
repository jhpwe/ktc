#ifndef __KTC_UTILS_H__
#define __KTC_UTILS_H__

#include <stdlib.h>
#include <asm/types.h>
#include <linux/if_ether.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/pkt_sched.h>
#include <limits.h>
#include <errno.h>
#include <arpa/inet.h>

#define TCA_BUF_MAX			(64*1024)
#define TIME_UNITS_PER_SEC	1000000
#define MAX_MSG 16384

/* iprotue2/tc/tc_util.c */
int get_rate64(__u64 *rate, const char *str);


/* iprotue2/lib/utils.c */
int __get_hz(void);

extern int __iproute2_hz_internal; 
static __inline__ int get_hz(void)
{
	if (__iproute2_hz_internal == 0)
		__iproute2_hz_internal = __get_hz();
	return __iproute2_hz_internal;
}

int get_be16(__be16 *val, const char *arg, int base);
int get_u32(__u32 *val, const char *arg, int base);
int get_u16(__u16 *val, const char *arg, int base);

#endif /* __KTC_UTILS_H__ */
