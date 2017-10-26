#ifndef __KTC_TC_HEADER__
#define __KTC_TC_HEADER__

#include <sys/types.h>
#include <asm/types.h>

enum ktc_cls_flags {
	KTC_CREATE_CLASS,
	KTC_CHANGE_CLASS,
	KTC_DELETE_CLASS,
	KTC_CHANGE_DEFUALT
};

int check_pid(char* pid);
int qdisc_init(char* dev, __u32 parent, __u32 defcls);
int cls_modify(char* dev, __u32 parent, __u32 clsid, char* rate, char* ceil, unsigned int cls_flag, __u64 res_gurantee);
int cls_show(char* dev);
int filter_add(char* dev, __u32 parent, char* _prio, char* handle);

int cgroup_init(void);
int cgroup_proc_add(char* pid, __u32 clsid);
int cgroup_proc_del(char* pid);

#endif
