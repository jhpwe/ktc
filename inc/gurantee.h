#ifndef __GURANTEE_H__
#define __GURANTEE_H__

#include <list.h>
#include <asm/types.h>
#include <sys/types.h>

struct pinfo 
{
	struct list_head		list;
	pid_t					pid;

	__u32					gurantee;
};

struct clsinfo 
{
	struct list_head		list;
	__u32					clsid;		// Class id

	__u32					rate;		// Bandwidth rate
	__u32					ceil;		// Bandwidth ceil
	__u32					gurantee;	// Guranteed bandwidth rate
	struct pinfo*			pinfos;
};

#endif
