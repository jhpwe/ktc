#include <stdio.h>
#include <string.h>
#include <asm/param.h>
#include <linux/atm.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

/* iprotue2/tc/tc_util.c */
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


/* iprotue2/lib/utils.c */
int __iproute2_hz_internal;

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

void print_rate(char *buf, int len, __u64 rate)
{
	unsigned long kilo = 0 ? 1024 : 1000;
	const char *str = 0 ? "i" : "";
	static char *units[5] = {"", "K", "M", "G", "T"};
	int i;

	rate <<= 3; /* bytes/sec -> bits/sec */

	for (i = 0; i < ARRAY_SIZE(units) - 1; i++)  {
		if (rate < kilo)
			break;
		if (((rate % kilo) != 0) && rate < 1000*kilo)
			break;
		rate /= kilo;
	}

	snprintf(buf, len, "%.0f%s%sbit", (double)rate, units[i], str);
}

char *sprint_rate(__u64 rate, char *buf)
{
	print_rate(buf, SPRINT_BSIZE-1, rate);
	return buf;
}

void print_size(char *buf, int len, __u32 sz)
{
	double tmp = sz;

	if (sz >= 1024*1024 && fabs(1024*1024*rint(tmp/(1024*1024)) - sz) < 1024)
		snprintf(buf, len, "%gMb", rint(tmp/(1024*1024)));
	else if (sz >= 1024 && fabs(1024*rint(tmp/1024) - sz) < 16)
		snprintf(buf, len, "%gKb", rint(tmp/1024));
	else
		snprintf(buf, len, "%ub", sz);
}

char *sprint_size(__u32 size, char *buf)
{
	print_size(buf, SPRINT_BSIZE-1, size);
	return buf;
}

int print_tc_classid(char *buf, int blen, __u32 h)
{
	SPRINT_BUF(handle) = {};
	int hlen = SPRINT_BSIZE - 1;

	if (h == TC_H_ROOT)
		sprintf(handle, "root");
	else if (h == TC_H_UNSPEC)
		snprintf(handle, hlen, "none");
	else if (TC_H_MAJ(h) == 0)
		snprintf(handle, hlen, ":%x", TC_H_MIN(h));
	else if (TC_H_MIN(h) == 0)
		snprintf(handle, hlen, "%x:", TC_H_MAJ(h) >> 16);
	else
		snprintf(handle, hlen, "%x:%x", TC_H_MAJ(h) >> 16, TC_H_MIN(h));

		snprintf(buf, blen, "%s", handle);

	return 0;
}

int get_tc_classid(__u32 *h, const char *str)
{
	__u32 maj, min;
	char *p;

	maj = TC_H_ROOT;
	if (strcmp(str, "root") == 0)
		goto ok;
	maj = TC_H_UNSPEC;
	if (strcmp(str, "none") == 0)
		goto ok;
	maj = strtoul(str, &p, 16);
	if (p == str) {
		maj = 0;
		if (*p != ':')
			return -1;
	}
	if (*p == ':') {
		if (maj >= (1<<16))
			return -1;
		maj <<= 16;
		str = p+1;
		min = strtoul(str, &p, 16);
		if (*p != 0)
			return -1;
		if (min >= (1<<16))
			return -1;
		maj |= min;
	} else if (*p != 0)
		return -1;

ok:
	*h = maj;
	return 0;
}

int ktclog(char* path, struct ktc_mq_s* ktc_msg, char* comment) 
{
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	char* home;
	FILE* logfile;

	if((logfile = fopen(path, "a")) < 0 ) 
	{
		perror("log file ");	
		exit(0);
	}

	fprintf(logfile, "%d [%2d-%2d-%2d/%2d:%2d:%2d] ", getpid(), tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	if(ktc_msg) fprintf(logfile, "%s from %s bound(%s ~ %s) ", ktc_msg->cmd, ktc_msg->pid, ktc_msg->lower, ktc_msg->upper);
	if(comment)	fprintf(logfile, "%s", comment);
	fprintf(logfile, "\n");
	
	fclose(logfile);

	return 0;
}

