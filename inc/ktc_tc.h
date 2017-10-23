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

int qdisc_init(char* dev, __u32 parent, __u32 defcls);
int cls_modify(char* dev, __u32 parent, __u32 clsid, char* rate, char* ceil, unsigned int cls_flag, __u64 res_gurantee);
int cls_show(char* dev);
int filter_add(char* dev, __u32 parent, char* _prio, char* handle);

int cgroup_init(void);

int ktc_proc_insert(char* dev, char* pid, char* low, char* high, char* link_speed);
int ktc_proc_change(char* dev, char* pid, char* low, char* high, char* link_speed);
int ktc_proc_delete(char* dev, char* pid, char* link_speed);

#endif
