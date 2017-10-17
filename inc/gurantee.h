#ifndef __GURANTEE_H__
#define __GURANTEE_H__

#include <list.h>
#include <asm/types.h>
#include <sys/types.h>

struct pinfo 
{
	struct list_head		list;
	char					pid[8];

	__u64					gurantee;
};

struct clsinfo 
{
	struct list_head		list;
	__u32					clsid;		// Class id

	__u64					rate;		// Bandwidth rate
	__u64					ceil;		// Bandwidth ceil
	__u64					gurantee;	// Guranteed bandwidth rate
	struct pinfo*			pinfos;
};


void clsinfo_show();
int clsinfo_add(__u32 clsid, char* pid, __u32 rate, __u32 ceil, __u32 gurantee);
int clsinfo_add_pid(__u32 clsid, char* pid);
struct clsinfo* clsinfo_create_cls(__u32 clsid, __u64 rate, __u64 ceil, __u64 gurantee);
int clsinfo_del_pid(char* pid);


void clsinfo_init(__u32 parent, __u32 defid, char* total, __u32 rate, __u32 ceil);
__u32 clsinfo_check_pid(char* pid);
__u32 clsinfo_create_clsid(void);
__u64 clsinfo_pid_add(char* pid, __u32 clsid, char* rate, char* ceil, char* gurantee);
__u64 clsinfo_pid_change(char* pid, __u32 clsid, char* rate, char* ceil, char* gurantee);
__u64 clsinfo_pid_delete(char* pid, __u32 clsid);

#endif
