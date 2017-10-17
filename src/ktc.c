#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>

#include "tc_core.h"
#include "libnetlink.h"
#include "ll_map.h"
#include "utils.h"
#include "gurantee.h"

#define NET_CLS_PATH "/sys/fs/cgroup/net_cls/"

enum ktc_cls_flags {
	KTC_CREATE_CLASS,
	KTC_CHANGE_CLASS,
	KTC_DELETE_CLASS,
};

struct req_s 
{
	struct nlmsghdr	n;
	struct tcmsg	t;
	char			buf[TCA_BUF_MAX];
};

//static __u32 parent = 0x010000;
//static __u32 defcls = 0x01;

/* Adding defualt htb qdisc
* $ tc qdisc add dev <dev_name> root handle <root_id> htb default <default_id>
**/
int qdisc_init(char* dev, __u32 parent, __u32 defcls)
{
	struct rtnl_handle rth;
	struct req_s req;
	struct rtattr *tail;
	
	struct tc_htb_glob opt = {
	.rate2quantum = 10,
	.version = 3,
	};

	if (rtnl_open(&rth, 0) < 0) {
		fprintf(stderr, "Cannot open rtnetlink\n");
		exit(1);
	}

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_EXCL | NLM_F_CREATE;
	req.n.nlmsg_type = RTM_NEWQDISC;
	req.t.tcm_family = AF_UNSPEC;
	req.t.tcm_parent = TC_H_ROOT;							// "root" qdisc as root
	req.t.tcm_handle = parent; 								// "handle handle:" root handle '1' --> '0x010000'
	addattr_l(&req.n, sizeof(req), TCA_KIND, "htb", 4);		// "htb" qdisc kind
	opt.defcls = defcls; 									// "default defcls" setting default class id to '0x01'

	tail = NLMSG_TAIL(&req.n);
	addattr_l(&req.n, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(&req.n, 2024, TCA_HTB_INIT, &opt, NLMSG_ALIGN(sizeof(opt)));
	tail->rta_len = (void *) NLMSG_TAIL(&req.n) - (void *) tail;

	/* finding device */
	if (dev[0]) {
		int idx;

		ll_init_map(&rth);

		idx = ll_name_to_index(dev);
		if (idx == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n", dev);
			return 1;
		}
		req.t.tcm_ifindex = idx;
	}

	if (rtnl_talk(&rth, &req.n, NULL, 0) < 0) {
		return 2;
	}

	rtnl_close(&rth);

	return 0;
}

/* Modify class to root qdisc
* $ tc class add dev <dev_name> parent <parent_id> classid <parent_id>:<class_id> htb rate <rate> ceil <ceil>
* $ tc class change dev <dev_name> parent <parent_id> classid <parent_id>:<class_id> htb rate <rate> ceil <ceil>
* $ tc class delete dev <dev_name> classid <parent_id>:<class:id>
**/
int cls_modify(char* dev, __u32 parent, __u32 clsid, char* rate, char* ceil, unsigned int cls_flag)
{
	struct rtnl_handle rth;
	struct req_s req;
	struct rtattr *tail;
	
	struct tc_htb_opt opt = {};
	__u64 ceil64 = 0, rate64 = 0;
	unsigned buffer = 0, cbuffer = 0;
	unsigned int mtu = 1600; /* eth packet len */
	unsigned short mpu = 0;
	unsigned short overhead = 0;
	unsigned int linklayer = LINKLAYER_ETHERNET; /* Assume ethernet */
	int cell_log =  -1, ccell_log = -1;
	__u32 rtab[256], ctab[256];

	if (rtnl_open(&rth, 0) < 0) {
		fprintf(stderr, "Cannot open rtnetlink\n");
		exit(1);
	}

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
	req.t.tcm_family = AF_UNSPEC;

	req.t.tcm_parent = parent; // "parent 1:" parent qidsc handle number
	req.t.tcm_handle = clsid;  // "classid 1:1" class id as 1:1
	/* Setting nlmsg by specific flags */
	switch(cls_flag)
	{
		case KTC_CREATE_CLASS :
			req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_EXCL|NLM_F_CREATE;
			req.n.nlmsg_type = RTM_NEWTCLASS;
			req.t.tcm_parent = parent; // "parent 1:" parent qidsc handle number
			break;
		case KTC_CHANGE_CLASS :
			req.n.nlmsg_flags = NLM_F_REQUEST | 0;
			req.n.nlmsg_type = RTM_NEWTCLASS;
			req.t.tcm_parent = parent; // "parent 1:" parent qidsc handle number
			break;
		case KTC_DELETE_CLASS : 
			req.n.nlmsg_flags = NLM_F_REQUEST | 0;
			req.n.nlmsg_type = RTM_DELTCLASS;
			break;
	}
	req.t.tcm_handle = clsid; // "classid 1:1" class id as 1:1

	if(cls_flag != KTC_DELETE_CLASS)
	{
		addattr_l(&req.n, sizeof(req), TCA_KIND, "htb", 4);

		get_rate64(&rate64, rate);	// "rate ##mbps"
		get_rate64(&ceil64, ceil);	// "ceil ##mbps"
		opt.rate.rate = (rate64 >= (1ULL << 32)) ? ~0U : rate64;
		opt.ceil.rate = (ceil64 >= (1ULL << 32)) ? ~0U : ceil64;

		clsinfo_create_cls(clsid, rate64, ceil64, rate64);

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
	}

	/* finding device */
	if (dev[0])  {
		ll_init_map(&rth);

		if ((req.t.tcm_ifindex = ll_name_to_index(dev)) == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n", dev);
			return 1;
		}
	}

	if (rtnl_talk(&rth, &req.n, NULL, 0) < 0) {
		return 2;
	}

	rtnl_close(&rth);

	return 0;
}

/* Adding cgroup filter to class, don't configuring class id like other filters
* $ tc filter add dev <dev_name> parent <parent_id>: protocol ip prio <prio> handle <class_id>: cgroup
**/
int filter_add(char* dev, __u32 parent, char* _prio, char* handle)
{
	struct rtnl_handle rth;
	struct req_s req;
	struct rtattr *tail;

	int protocol_set = 0;
	__u32 prio = 0;
	__u32 protocol = 0;
	__u16 id;
	struct tcmsg *t;
	long h = 0;

	if (rtnl_open(&rth, 0) < 0) {
		fprintf(stderr, "Cannot open rtnetlink\n");
		exit(1);
	}

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_EXCL|NLM_F_CREATE;
	req.n.nlmsg_type = RTM_NEWTFILTER;
	req.t.tcm_family = AF_UNSPEC;
	req.t.tcm_parent = parent;						// "parent 1:"

	extern int ll_proto_a2n(unsigned short *id, const char *buf);
	ll_proto_a2n(&id, "ip");						// "protocol ip"
	protocol = id;
	protocol_set = 1;
	get_u32(&prio, _prio, 0); 						// "prio"
	req.t.tcm_info = TC_H_MAKE(prio<<16, protocol); // "cgroup"
	addattr_l(&req.n, sizeof(req), TCA_KIND, "cgroup", 7);

	h = strtol(handle, NULL, 0);					// "handle"
	if (h == LONG_MIN || h == LONG_MAX) {
		fprintf(stderr, "Illegal handle \"%s\", must be numeric.\n", handle);
	}
	t = NLMSG_DATA(&req.n);
	t->tcm_handle = h;

	tail = (struct rtattr *)(((void *)&req.n)+NLMSG_ALIGN(req.n.nlmsg_len));
	addattr_l(&req.n, MAX_MSG, TCA_OPTIONS, NULL, 0);

	tail->rta_len = (((void *)&req.n)+req.n.nlmsg_len) - (void *)tail;

	if (dev[0])  {
		ll_init_map(&rth);

		req.t.tcm_ifindex = ll_name_to_index(dev);
		if (req.t.tcm_ifindex == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n", dev);
			return 1;
		}
	}

	if (rtnl_talk(&rth, &req.n, NULL, 0) < 0) {
		fprintf(stderr, "We have an error talking to the kernel\n");
		return 2;
	}

	rtnl_close(&rth);

	return 0;
}

int cgroup_init(void)
{
	return 0;
}

int cgroup_proc_add(char* pid, __u32 clsid)
{
	char path[64] = {};
	FILE *fp = NULL;

	/* Make <pid> directory in net_cls directory. */
	sprintf(path, "%s%s", NET_CLS_PATH, pid);
	if(mkdir(path, 0755) != 0)
	{
		printf("Failed to make process directory in cgroup.\n");
		return -1;
	}

	/* Open <pid>/cgroup.proc file. */
	sprintf(path, "%s%s", path, "/cgroup.procs");
	if( (fp = fopen(path, "r+")) == NULL)
	{
		printf("Failed to open procs file.\n");
		return -1;
	}

	/* Add <pid> in <pid>/cgroup.procs file. */
	if(fprintf(fp, "%s", pid) != 0)
	{
		printf("Failed to add PID in %s/cgroup.procs\n", pid);
		fclose(fp);
		return -1;
	}
	fclose(fp);

	return 0;
}

int cgroup_proc_del(char* pid)
{
	char path[64] = {};
	FILE *fp = NULL;

	/* Open net_cls/cgroup.procs file. */
	sprintf(path, "%s%s", NET_CLS_PATH, "cgroup.procs");
	if( (fp = fopen(path, "r+")) == NULL)
	{
		printf("Failed to open procs file.\n");
		return -1;
	}

	fseek(fp, 0L, SEEK_END);
	
	/* Add <pid> in net_cls/cgroup.procs file. */
	if(fprintf(fp, "\n%s", pid) != 0)
	{
		printf("Failed to add PID in net_cls/cgroup.procs\n");
		fclose(fp);
		return -1;
	}
	fclose(fp);
	
	/* Remove <pid> directory in net_cls directory. */
	sprintf(path, "%s%s", NET_CLS_PATH, pid);
	if(rmdir(path) != 0)
	{
		printf("Failed to remove process directory in cgroup.\n");
		return -1;
	}

	return 0;
}

int main(int argc, char** argv)
{
	int sel;

	if(access("/", R_OK | W_OK) != 0) {
		printf("Must run as root.\n");
		return -1;
	}

	if(argc < 2)
	{
		printf("Need to input argument.\n");
		return -1;
	}

	sel = atoi(argv[1]);

	cgroup_init();
	clsinfo_init(0x01, 1000000000, 1000000000);

	while(sel != 0) {
		switch(sel)
		{
			case 1:
				qdisc_init("wlp2s0", 0x010000, 0x1);
				break;
			case 2:
				cls_modify("wlp2s0", 0x010000, 0x010001, "15mbps", "20mbps", KTC_CREATE_CLASS);
				break;
			case 3:
				filter_add("wlp2s0", 0x010000, "10", "1:");
				break;
			case 4:
				cls_modify("wlp2s0", 0x010000, 0x010001, "5mbps", "5mbps", KTC_CHANGE_CLASS);
				break;
			case 5:
				cls_modify("wlp2s0", 0, 0x010001, NULL, NULL, KTC_DELETE_CLASS);
				break;
			case 6:
				cgroup_proc_add("2075", 0x010001);
				break;
			case 7:
				cgroup_proc_del("2075");
				break;
			case 8:
				clsinfo_show();
				break;
		}
		clsinfo_show();
		scanf("%d", &sel);
	}
	
	return 0;
}

