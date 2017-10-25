#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include "ktc_tc.h"

#include "tc_core.h"
#include "libnetlink.h"
#include "ll_map.h"
#include "utils.h"
#include "gcls.h"

#define NET_CLS_PATH "/sys/fs/cgroup/net_cls/"

struct req_s
{
	struct nlmsghdr	n;
	struct tcmsg	t;
	char			buf[TCA_BUF_MAX];
};

extern int ktclog(struct ktc_mq_s* ktc_msg, char* comment);

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

int _print_class(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	FILE *fp = (FILE *)arg;
	struct tcmsg *t = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr *tb[TCA_MAX + 1];
	char abuf[256];

	struct rtattr *tb_opt[TCA_HTB_MAX + 1];
	struct tc_htb_opt *hopt;
	struct tc_htb_glob *gopt;
	double buffer, cbuffer;
	__u64 rate64, ceil64;

	char logbuf[256] = {};

	SPRINT_BUF(b1);
	SPRINT_BUF(b2);

	if (n->nlmsg_type != RTM_NEWTCLASS && n->nlmsg_type != RTM_DELTCLASS) {
		//fprintf(stderr, "Not a class\n");
		return 0;
	}
	len -= NLMSG_LENGTH(sizeof(*t));
	if (len < 0) {
		//fprintf(stderr, "Wrong len %d\n", len);
		return -1;
	}

	parse_rtattr(tb, TCA_MAX, TCA_RTA(t), len);

	if (tb[TCA_KIND] == NULL) {
		//fprintf(stderr, "print_class: NULL kind\n");
		return -1;
	}

	if (n->nlmsg_type == RTM_DELTCLASS)
		//fprintf(fp, "deleted ");

	abuf[0] = 0;
	if (t->tcm_handle) {
			print_tc_classid(abuf, sizeof(abuf), t->tcm_handle);
	}
	//fprintf(fp, "class %s %s ", rta_getattr_str(tb[TCA_KIND]), abuf);
	sprintf(logbuf, "class %s %s ", rta_getattr_str(tb[TCA_KIND]), abuf);

	if (t->tcm_parent == TC_H_ROOT) {
		//fprintf(fp, "root ");
	}
	else {
		print_tc_classid(abuf, sizeof(abuf), t->tcm_parent);
		//fprintf(fp, "parent %s ", abuf);
	}

	if( strncmp(RTA_DATA(tb[TCA_KIND]), "htb", 3) == 0);
		//_print_opt(fp, tb[TCA_OPTIONS]);



		/* _print_opt */
		if (tb[TCA_OPTIONS] == NULL)
			return 0;

		parse_rtattr_nested(tb_opt, TCA_HTB_MAX, tb[TCA_OPTIONS]);

		if (tb_opt[TCA_HTB_PARMS]) {
			hopt = RTA_DATA(tb_opt[TCA_HTB_PARMS]);
			if (RTA_PAYLOAD(tb_opt[TCA_HTB_PARMS])  < sizeof(*hopt))
				return -1;

			if (!hopt->level) {
				//fprintf(fp, "prio %d ", (int)hopt->prio);
			}

			/* calc rate */
			rate64 = hopt->rate.rate;
			if (tb_opt[TCA_HTB_RATE64] && RTA_PAYLOAD(tb_opt[TCA_HTB_RATE64]) >= sizeof(rate64))
				rate64 = rta_getattr_u64(tb_opt[TCA_HTB_RATE64]);

			/* calc ceil */
			ceil64 = hopt->ceil.rate;
			if (tb_opt[TCA_HTB_CEIL64] && RTA_PAYLOAD(tb_opt[TCA_HTB_CEIL64]) >= sizeof(ceil64))
				ceil64 = rta_getattr_u64(tb_opt[TCA_HTB_CEIL64]);

			sprintf(logbuf, "%srate %s ceil %s", logbuf, sprint_rate(rate64, b1), sprint_rate(ceil64, b1));

			//fprintf(fp, "rate %s ", sprint_rate(rate64, b1));
			//fprintf(fp, "ceil %s ", sprint_rate(ceil64, b1));
		}

	//fprintf(fp, "\n");
	ktclog(NULL, logbuf);

	//fflush(fp);

	return 0;
}

int cls_show(char* dev)
{
	struct rtnl_handle rth = {};
	struct tcmsg t = { .tcm_family = AF_UNSPEC };
	char buf[1024] = {0};

	if (rtnl_open(&rth, 0) < 0) {
		fprintf(stderr, "Cannot open rtnetlink\n");
		exit(1);
	}

	ll_init_map(&rth);

	if (dev[0]) {
		if ((t.tcm_ifindex = ll_name_to_index(dev)) == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n", dev);
			return 1;
		}
	}

		if (rtnl_dump_request(&rth, RTM_GETTCLASS, &t, sizeof(t)) < 0) {
			perror("Cannot send dump request");
			return 1;
		}

		if (rtnl_dump_filter(&rth, _print_class, stdout) < 0) {
			fprintf(stderr, "Dump terminated\n");
			return 1;
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

int cgroup_init(void) {
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
