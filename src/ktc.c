#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mqueue.h>

#include "tc_core.h"
#include "libnetlink.h"
#include "ll_map.h"
#include "utils.h"
#include "gcls.h"

#define NET_CLS_PATH "/sys/fs/cgroup/net_cls/"
#define KTC_MQ_PATH "/ktc_mq"

enum ktc_cls_flags {
	KTC_CREATE_CLASS,
	KTC_CHANGE_CLASS,
	KTC_DELETE_CLASS,
	KTC_CHANGE_DEFUALT
};

struct req_s
{
	struct nlmsghdr	n;
	struct tcmsg	t;
	char			buf[TCA_BUF_MAX];
};

struct ktc_mq_s
{
  char cmd[8];
  char pid[8];
  char upper[16];
  char lower[16];
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
int cls_modify(char* dev, __u32 parent, __u32 clsid, char* rate, char* ceil, unsigned int cls_flag, __u64 res_gurantee)
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
		case KTC_CHANGE_DEFUALT :
			req.n.nlmsg_flags = NLM_F_REQUEST | 0;
			req.n.nlmsg_type = RTM_NEWTCLASS;
			req.t.tcm_parent = parent; // "parent 1:" parent qidsc handle number
			break;
	}
	req.t.tcm_handle = clsid; // "classid 1:1" class id as 1:1

	if(cls_flag != KTC_DELETE_CLASS)
	{
		addattr_l(&req.n, sizeof(req), TCA_KIND, "htb", 4);

		if(cls_flag != KTC_CHANGE_DEFUALT)
			get_rate64(&rate64, rate);	// "rate ##mbps"
		else
			rate64 = res_gurantee;

		get_rate64(&ceil64, ceil);	// "ceil ##mbps"
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

/* Check PID */
int check_pid(char* pid)
{
	char path[16] = {};
	sprintf(path, "/proc/%s", pid);

	return access(path, R_OK);
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
		printf("Failed to make %s directory.\n", path);
		return -1;
	}

	/* Open <pid>/cgroup.procs file. */
	sprintf(path, "%s%s", path, "/cgroup.procs");
	if( (fp = fopen(path, "a")) == NULL)
	{
		printf("Failed to open %s.\n", path);
		return -1;
	}

	/* Add <pid> in <pid>/cgroup.procs file. */
	if(fprintf(fp, "%s", pid) < 0)
	{
		printf("Failed to add %s in %s.\n", pid, path);
		fclose(fp);
		return -1;
	}
	fclose(fp);

	/* Open <pid>/net_cls.classid */
	sprintf(path, "%s%s%s", NET_CLS_PATH, pid, "/net_cls.classid");
	if( (fp = fopen(path, "a")) == NULL)
	{
		printf("Failed to open %s.\n", path);
		return -1;
	}

	/* Add <clsid> in <pid>/net_cls.classid file. */
	if(fprintf(fp, "0x%x", clsid) < 0)
	{
		printf("Failed to add 0x%x in %s.\n", clsid, path);
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
	if( (fp = fopen(path, "a")) == NULL)
	{
		printf("Failed to open %s.\n", path);
		return -1;
	}

	/* Add <pid> in net_cls/cgroup.procs file. */
	if(fprintf(fp, "\n%s", pid) < 0)
	{
		printf("Failed to add %s in %s.\n", pid, path);
		fclose(fp);
		return -1;
	}
	fclose(fp);

	/* Remove <pid> directory in net_cls directory. */
	sprintf(path, "%s%s", NET_CLS_PATH, pid);
	if(rmdir(path) != 0)
	{
		printf("Failed to remove %s directory.\n", path);
		return -1;
	}

	return 0;
}

int ktc_proc_insert(char* dev, char* pid, char* low, char* high, char* link_speed)
{
	__u32 clsid;
	__u64 def_rate;

	if(check_pid(pid))
	{
		printf("PID %s is not running.\n", pid);
		return -1;
	}

	if(gcls_check_pid(pid) > 0)
	{
		printf("PID %s is already exist in list.\n", pid);
		return -1;
	}

	clsid = gcls_empty_id();

	printf("0x%x\n",clsid);

	if( (def_rate = gcls_add(clsid, pid, low, high) ) == ULONG_MAX)
	{
		printf("total rate over the max link speed\n");
		return -1;
	}

	cgroup_proc_add(pid, clsid);

	cls_modify(dev, 0x010001, clsid, low, high, KTC_CREATE_CLASS, 0);

	cls_modify(dev, 0x010001, 0x010002, 0, link_speed, KTC_CHANGE_DEFUALT, def_rate);

	return 0;
}

int ktc_proc_change(char* dev, char* pid, char* low, char* high, char* link_speed)
{
	__u32 clsid;
	__u64 def_rate;

	if(check_pid(pid))
	{
		printf("PID %s is not running.\n", pid);
		return -1;
	}

	if( (clsid = gcls_check_pid(pid)) == 0)
	{
		printf("PID %s is not exist in list.\n", pid);
		return -1;
	}

	if( (def_rate = gcls_modify(clsid, pid, low, high) ) == ULONG_MAX)
	{
		printf("total rate over the max link speed\n");
		return -1;
	}

	cls_modify(dev, 0x010001, clsid, low, high, KTC_CHANGE_CLASS, 0);

	cls_modify(dev, 0x010001, 0x010002, 0, link_speed, KTC_CHANGE_DEFUALT, def_rate);

	return 0;
}

int ktc_proc_delete(char* dev, char* pid, char* link_speed)
{
	__u32 clsid;
	__u64 def_rate;

	if(check_pid(pid))
	{
		printf("PID %s is not running.\n", pid);
		printf("Force delete in list.\n");
	//	return -1;
	}

	if( (clsid = gcls_check_pid(pid)) == 0)
	{
		printf("PID %s is not exist in list.\n", pid);
		return -1;
	}

	if( (def_rate = gcls_delete_pid(pid) ) == ULONG_MAX ) //fail < 0, success = res gurantee
	{
		printf("Something wrong.\n");
		return -1;
	}

	cgroup_proc_del(pid);

	cls_modify(dev, 0, clsid, NULL, NULL, KTC_DELETE_CLASS, 0);

	cls_modify(dev, 0x010001, 0x010002, 0, link_speed, KTC_CHANGE_DEFUALT, def_rate);

	return 0;
}

mqd_t msgq_init(struct mq_attr* attr)
{
	mqd_t mfd;

	attr->mq_maxmsg = 10;
	attr->mq_msgsize = sizeof(struct ktc_mq_s);

	mfd = mq_open(KTC_MQ_PATH, O_RDWR | O_CREAT,  0666, attr);
	if (mfd == -1)
	{
		return -1;
	}
	return mfd;
}

int msgq_get(mqd_t mfd, struct mq_attr* attr, struct ktc_mq_s* kmq)
{
	if((mq_receive(mfd, (char *)kmq, attr->mq_msgsize,NULL)) == -1)
	{
		return -1;
	}
	else
	{
		printf("received : %s %s\n",kmq->cmd, kmq->pid);
	}
	return 0;
}

void usage(void)
{
	printf("Usage: ktc dev [DEVICE NAME] link [MAX LINK SPEED]\n");
}

int main(int argc, char** argv)
{
	char dev[16] = {};
	char link_speed[16] = {};

	mqd_t mfd;
	struct mq_attr attr;
	struct ktc_mq_s kmq;

	if(access("/", R_OK | W_OK) != 0) {
		printf("Must run as root.\n");
		return -1;
	}

	if(argc != 5)
	{
		usage();
		return -1;
	}

	argc--;
	argv++;

	while(argc > 0)
	{
		if(strcmp(*argv, "dev") == 0)
		{
			argc--;
			argv++;
			strncpy(dev, *argv, sizeof(dev)-1);
		}
		else if(strcmp(*argv, "link") == 0)
		{
			argc--;
			argv++;
			strncpy(link_speed, *argv, sizeof(dev)-1);
		}
		else
		{
			printf("Unknown argument : %s\n", *argv);
			usage();
			return -1;
		}
		argc--;
		argv++;
	}

	cgroup_init();
	qdisc_init(dev, 0x010000, 0x2);

	/* root class */
	cls_modify(dev, 0x010000, 0x010001, link_speed, link_speed, KTC_CREATE_CLASS, 0);
	/* default class */
	cls_modify(dev, 0x010001, 0x010002, link_speed, link_speed, KTC_CREATE_CLASS, 0);

	filter_add(dev, 0x010000, "10", "1:");
	gcls_init(0x010000, 0x010002, link_speed);

	if( (mfd = msgq_init(&attr)) < 0)
	{
		printf("msgq open error\n");
		return -1;
	}

	while(1)
	{
		if(msgq_get(mfd, &attr, &kmq) < 0)
		{
			printf("msgq get error\n");
			return -1;
		}

		if(strcmp(kmq.cmd, "add") == 0)
		{
			ktc_proc_insert(dev, kmq.pid, kmq.lower, kmq.upper, link_speed);
		}
		else if(strcmp(kmq.cmd, "change") == 0)
		{
			ktc_proc_change(dev, kmq.pid, kmq.lower, kmq.upper, link_speed);
		}
		else if(strcmp(kmq.cmd, "delete") == 0)
		{
			ktc_proc_delete(dev, kmq.pid, link_speed);
		}
	}


/*
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
				cgroup_proc_add("4049", 0x010001);
				break;
			case 7:
				cgroup_proc_del("4049");
				break;
			case 8:
				clsinfo_show();
				break;
		}
		scanf("%d", &sel);
	}
*/

	return 0;
}
