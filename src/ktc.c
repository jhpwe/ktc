#include <sys/socket.h>

#include "tc_core.h"
#include "libnetlink.h"
#include "ll_map.h"
#include "utils.h"

struct req_s 
{
	struct nlmsghdr	n;
	struct tcmsg	t;
	char			buf[TCA_BUF_MAX];
};

//tc qdisc add dev wlp2s0 root handle 1: htb default 1
int qdisc_init(char* dev)
{
	struct rtnl_handle rth;
	struct req_s req;
	struct rtattr *tail;

	char dev_name[16] = "wlp2s0";
	
	struct tc_htb_glob opt = {
	.rate2quantum = 10,
	.version = 3,
	};

	if (rtnl_open(&rth, 0) < 0) {
		fprintf(stderr, "Cannot open rtnetlink\n");
		exit(1);
	}
	else
	{
		printf("opened\n");
	}

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_EXCL | NLM_F_CREATE;
	req.n.nlmsg_type = RTM_NEWQDISC;
	req.t.tcm_family = AF_UNSPEC;

	//root
	req.t.tcm_parent = TC_H_ROOT;

	//handle 1:
	req.t.tcm_handle = 0x10000; // "1:"

	//htb
	addattr_l(&req.n, sizeof(req), TCA_KIND, "htb", 4);

	//default
	opt.defcls = 0x1; // classid is :1

	tail = NLMSG_TAIL(&req.n);
	addattr_l(&req.n, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(&req.n, 2024, TCA_HTB_INIT, &opt, NLMSG_ALIGN(sizeof(opt)));
	tail->rta_len = (void *) NLMSG_TAIL(&req.n) - (void *) tail;

	//dev
	if (dev_name[0])  {
		int idx;

		ll_init_map(&rth);

		idx = ll_name_to_index(dev_name);
		if (idx == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n", dev_name);
			return 1;
		}
		req.t.tcm_ifindex = idx;
	}

	if (rtnl_talk(&rth, &req.n, NULL, 0) < 0)
	{
		return 2;
	}

	rtnl_close(&rth);

	return 0;
}


//class add dev dev_name parent 1: classid 1:2 htb rate 15mbit ceil ~~
int cls_add(char* dev)
{
	struct rtnl_handle rth;
	struct req_s req;
	struct rtattr *tail;

	char dev_name[16] = "wlp2s0";
	
	struct tc_htb_opt opt = {};
	__u64 ceil64 = 0, rate64 = 0;
	unsigned buffer = 0, cbuffer = 0;
	unsigned int mtu = 1600; /* eth packet len */
	unsigned short mpu = 0;
	unsigned short overhead = 0;
	unsigned int linklayer  = LINKLAYER_ETHERNET; /* Assume ethernet */
	int cell_log =  -1, ccell_log = -1;
	__u32 rtab[256], ctab[256];

	if (rtnl_open(&rth, 0) < 0) {
		fprintf(stderr, "Cannot open rtnetlink\n");
		exit(1);
	}
	else
	{
		printf("opened\n");
	}

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_EXCL | NLM_F_CREATE;
	req.n.nlmsg_type = RTM_NEWTCLASS;
	req.t.tcm_family = AF_UNSPEC;

	//parent 1:
	req.t.tcm_parent = 0x10000; // "1:"

	//classid 1:1
	req.t.tcm_handle = 0x10001; // "1:1"

	//htb
	addattr_l(&req.n, sizeof(req), TCA_KIND, "htb", 4);

	//rate and ceil
	get_rate64(&rate64, "15mbps");
	get_rate64(&ceil64, "20mbps");

	opt.rate.rate = (rate64 >= (1ULL << 32)) ? ~0U : rate64;
	opt.ceil.rate = (ceil64 >= (1ULL << 32)) ? ~0U : ceil64;

	if (!buffer)
		buffer = rate64 / get_hz() + mtu;
	if (!cbuffer)
		cbuffer = ceil64 / get_hz() + mtu;

	opt.ceil.overhead = overhead;
	opt.rate.overhead = overhead;

	opt.ceil.mpu = mpu;
	opt.rate.mpu = mpu;

	if (tc_calc_rtable(&opt.rate, rtab, cell_log, mtu, linklayer) < 0) {
		fprintf(stderr, "htb: failed to calculate rate table.\n");
		return -1;
	}
	opt.buffer = tc_calc_xmittime(rate64, buffer);

	if (tc_calc_rtable(&opt.ceil, ctab, ccell_log, mtu, linklayer) < 0) {
		fprintf(stderr, "htb: failed to calculate ceil rate table.\n");
		return -1;
	}
	opt.cbuffer = tc_calc_xmittime(ceil64, cbuffer);

	tail = NLMSG_TAIL(&req.n);
	addattr_l(&req.n, 1024, TCA_OPTIONS, NULL, 0);

	if (rate64 >= (1ULL << 32))
		addattr_l(&req.n, 1124, TCA_HTB_RATE64, &rate64, sizeof(rate64));

	if (ceil64 >= (1ULL << 32))
		addattr_l(&req.n, 1224, TCA_HTB_CEIL64, &ceil64, sizeof(ceil64));

	addattr_l(&req.n, 2024, TCA_HTB_PARMS, &opt, sizeof(opt));
	addattr_l(&req.n, 3024, TCA_HTB_RTAB, rtab, 1024);
	addattr_l(&req.n, 4024, TCA_HTB_CTAB, ctab, 1024);
	tail->rta_len = (void *) NLMSG_TAIL(&req.n) - (void *) tail;

	//dev
	if (dev_name[0])  {
		ll_init_map(&rth);

		if ((req.t.tcm_ifindex = ll_name_to_index(dev_name)) == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n", dev_name);
			return 1;
		}
	}

	if (rtnl_talk(&rth, &req.n, NULL, 0) < 0)
	{
		return 2;
	}

	rtnl_close(&rth);

	return 0;
}

//tc filter add dev eth0 parent 10: protocol ip prio 10 handle 1: cgroup
int filter_add(char* dev)
{
	struct rtnl_handle rth;
	struct req_s req;
	char dev_name[16] = "wlp2s0";
	int protocol_set = 0;
	char handle[16] = "1:";
	__u32 prio = 0;
	__u32 protocol = 0;
	__u16 id;
	struct tcmsg *t;
	struct rtattr *tail;
	long h = 0;

	if (rtnl_open(&rth, 0) < 0) {
		fprintf(stderr, "Cannot open rtnetlink\n");
		exit(1);
	}
	else
	{
		printf("opened\n");
	}

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_EXCL|NLM_F_CREATE;
	req.n.nlmsg_type = RTM_NEWTFILTER;
	req.t.tcm_family = AF_UNSPEC;

	//parent 1:
	req.t.tcm_parent = 0x10000;

	//protocol
	extern int ll_proto_a2n(unsigned short *id, const char *buf);
	ll_proto_a2n(&id, "ip");
	protocol = id;
	protocol_set = 1;

	//prio
	get_u32(&prio, "10", 0);

	//cgroup
	req.t.tcm_info = TC_H_MAKE(prio<<16, protocol);
	addattr_l(&req.n, sizeof(req), TCA_KIND, "cgroup", 7);

	//handle
	h = strtol(handle, NULL, 0);
	if (h == LONG_MIN || h == LONG_MAX) 
	{
		fprintf(stderr, "Illegal handle \"%s\", must be numeric.\n", handle);
	}
	t = NLMSG_DATA(&req.n);
	t->tcm_handle = h;

	tail = (struct rtattr *)(((void *)&req.n)+NLMSG_ALIGN(req.n.nlmsg_len));
	addattr_l(&req.n, MAX_MSG, TCA_OPTIONS, NULL, 0);

	tail->rta_len = (((void *)&req.n)+req.n.nlmsg_len) - (void *)tail;

	//dev
	if (dev_name[0])  {
		ll_init_map(&rth);

		req.t.tcm_ifindex = ll_name_to_index(dev_name);
		if (req.t.tcm_ifindex == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n", dev_name);
			return 1;
		}
	}

	//send
	if (rtnl_talk(&rth, &req.n, NULL, 0) < 0) {
		fprintf(stderr, "We have an error talking to the kernel\n");
		return 2;
	}

	rtnl_close(&rth);

	return 0;
}


int main(void)
{
	qdisc_init("wlp2s0");
	cls_add("wlp2s0");
	filter_add("wlp2s0");
	return 0;
}

