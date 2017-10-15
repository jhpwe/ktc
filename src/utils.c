#include <stdio.h>
#include <string.h>
#include <asm/param.h>
#include <linux/atm.h>

#include "utils.h"

const struct rate_suffix {
	const char *name;
	double scale;
} suffixes[] = {
	{ "bit",	1. },
	{ "Kibit",	1024. },
	{ "kbit",	1000. },
	{ "mibit",	1024.*1024. },
	{ "mbit",	1000000. },
	{ "gibit",	1024.*1024.*1024. },
	{ "gbit",	1000000000. },
	{ "tibit",	1024.*1024.*1024.*1024. },
	{ "tbit",	1000000000000. },
	{ "Bps",	8. },
	{ "KiBps",	8.*1024. },
	{ "KBps",	8000. },
	{ "MiBps",	8.*1024*1024. },
	{ "MBps",	8000000. },
	{ "GiBps",	8.*1024.*1024.*1024. },
	{ "GBps",	8000000000. },
	{ "TiBps",	8.*1024.*1024.*1024.*1024. },
	{ "TBps",	8000000000000. },
	{ NULL }

};

double tick_in_usec = 1;

int get_rate64(__u64 *rate, const char *str)
{
	char *p;
	double bps = strtod(str, &p);
	const struct rate_suffix *s;

	if (p == str)
		return -1;

	for (s = suffixes; s->name; ++s) {
		if (strcasecmp(s->name, p) == 0) {
			bps *= s->scale;
			p += strlen(p);
			break;
		}
	}

	if (*p)
		return -1; /* unknown suffix */

	bps /= 8; /* -> bytes per second */
	*rate = bps;
	return 0;
}

int __get_hz(void)
{
	char name[1024];
	int hz = 0;
	FILE *fp;

	if (getenv("HZ"))
		return atoi(getenv("HZ")) ? : HZ;

	if (getenv("PROC_NET_PSCHED")) {
		snprintf(name, sizeof(name)-1, "%s", getenv("PROC_NET_PSCHED"));
	} else if (getenv("PROC_ROOT")) {
		snprintf(name, sizeof(name)-1, "%s/net/psched", getenv("PROC_ROOT"));
	} else {
		strcpy(name, "/proc/net/psched");
	}
	fp = fopen(name, "r");

	if (fp) {
		unsigned nom, denom;
		if (fscanf(fp, "%*08x%*08x%08x%08x", &nom, &denom) == 2)
			if (nom == 1000000)
				hz = denom;
		fclose(fp);
	}
	if (hz)
		return hz;
	return HZ;
}

int tc_calc_rtable(struct tc_ratespec *r, __u32 *rtab,
		   int cell_log, unsigned int mtu,
		   enum link_layer linklayer)
{
	int i;
	unsigned int sz;
	unsigned int bps = r->rate;
	unsigned int mpu = r->mpu;

	if (mtu == 0)
		mtu = 2047;

	if (cell_log < 0) {
		cell_log = 0;
		while ((mtu >> cell_log) > 255)
			cell_log++;
	}

	for (i = 0; i < 256; i++) {
		sz = tc_adjust_size((i + 1) << cell_log, mpu, linklayer);
		rtab[i] = tc_calc_xmittime(bps, sz);
	}

	r->cell_align =  -1;
	r->cell_log = cell_log;
	r->linklayer = (linklayer & TC_LINKLAYER_MASK);
	return cell_log;
}

unsigned int tc_adjust_size(unsigned int sz, unsigned int mpu, enum link_layer linklayer)
{
	if (sz < mpu)
		sz = mpu;

	switch (linklayer) {
	case LINKLAYER_ATM:
		return tc_align_to_atm(sz);
	case LINKLAYER_ETHERNET:
	default:
		/* No size adjustments on Ethernet */
		return sz;
	}
}

unsigned int tc_calc_xmittime(__u64 rate, unsigned int size)
{
	return tc_core_time2tick(TIME_UNITS_PER_SEC*((double)size/(double)rate));
}

unsigned int tc_core_time2tick(unsigned int time)
{
	return time*tick_in_usec;
}

unsigned int tc_align_to_atm(unsigned int size)
{
	int linksize, cells;

	cells = size / ATM_CELL_PAYLOAD;
	if ((size % ATM_CELL_PAYLOAD) > 0)
		cells++;

	linksize = cells * ATM_CELL_SIZE; /* Use full cell size to add ATM tax */
	return linksize;
}


int ll_proto_a2n(unsigned short *id, const char *buf)
{
        int i;
        for (i=0; i < sizeof(llproto_names)/sizeof(llproto_names[0]); i++) {
                 if (strcasecmp(llproto_names[i].name, buf) == 0) {
			 *id = htons(llproto_names[i].id);
			 return 0;
		 }
	}
	if (get_be16(id, buf, 0))
		return -1;
	return 0;
}

int get_be16(__be16 *val, const char *arg, int base)
{
	__u16 v;
	int ret = get_u16(&v, arg, base);

	if (!ret)
		*val = htons(v);

	return ret;
}

int get_u32(__u32 *val, const char *arg, int base)
{
	unsigned long res;
	char *ptr;

	if (!arg || !*arg)
		return -1;
	res = strtoul(arg, &ptr, base);

	/* empty string or trailing non-digits */
	if (!ptr || ptr == arg || *ptr)
		return -1;

	/* overflow */
	if (res == ULONG_MAX && errno == ERANGE)
		return -1;

	/* in case UL > 32 bits */
	if (res > 0xFFFFFFFFUL)
		return -1;

	*val = res;
	return 0;
}

int get_u16(__u16 *val, const char *arg, int base)
{
	unsigned long res;
	char *ptr;

	if (!arg || !*arg)
		return -1;
	res = strtoul(arg, &ptr, base);

	/* empty string or trailing non-digits */
	if (!ptr || ptr == arg || *ptr)
		return -1;

	/* overflow */
	if (res == ULONG_MAX && errno == ERANGE)
		return -1;

	if (res > 0xFFFFUL)
		return -1;

	*val = res;
	return 0;
}