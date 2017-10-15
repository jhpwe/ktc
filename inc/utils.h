#ifndef __KTC_UTILS_H__
#define __KTC_UTILS_H__

#include <stdlib.h>
#include <asm/types.h>
#include <linux/rtnetlink.h>
#include <linux/netlink.h>
#include <linux/pkt_sched.h>

#define TCA_BUF_MAX			(64*1024)
#define TIME_UNITS_PER_SEC	1000000

enum link_layer
{
	LINKLAYER_UNSPEC,
	LINKLAYER_ETHERNET,
	LINKLAYER_ATM,
};

struct req_s 
{
	struct nlmsghdr	n;
	struct tcmsg	t;
	char			buf[TCA_BUF_MAX];
};

extern int __iproute2_hz_internal;

int get_rate64(__u64 *rate, const char *str);
int __get_hz(void);

static __inline__ int get_hz(void)
{
	if (__iproute2_hz_internal == 0)
		__iproute2_hz_internal = __get_hz();
	return __iproute2_hz_internal;
}

int tc_calc_rtable(struct tc_ratespec *r, __u32 *rtab, int cell_log, unsigned int mtu, enum link_layer linklayer);
unsigned int tc_adjust_size(unsigned int sz, unsigned int mpu, enum link_layer linklayer);
unsigned int tc_calc_xmittime(__u64 rate, unsigned int size);
unsigned int tc_core_time2tick(unsigned int time);
unsigned int tc_align_to_atm(unsigned int size);

#endif /* __KTC_UTILS_H__ */
