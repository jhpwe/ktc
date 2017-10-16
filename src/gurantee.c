#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <list.h>
#include "gurantee.h"

static __u32 TOTAL_RATE = 1000000000; // 1Gbit/sec
static struct clsinfo defcls;

void clsinfo_show() 
{
	struct clsinfo* cls_pos;
	struct list_head* cls_head = &defcls.list;
	int i = 0;

	printf("[DEFAULT]");

	list_for_each_entry(cls_pos, cls_head, list) {
		printf("id:0x%X / rate:0x%X / ceil: 0x%X / gurantee: 0x%X\n", 
				i, cls_pos->rate, cls_pos->ceil, cls_pos->gurantee);

		struct pinfo* p_pos;
		struct list_head* p_head = &cls_pos->pinfos->list;

		list_for_each_entry(p_pos, p_head, list) {
			printf("\tpid: 0x%s / gurantee: 0x%X\n", p_pos->pid, p_pos->gurantee);
		}
	}
}

void clsinfo_init(__u32 defid, __u32 rate, __u32 ceil) 
{
	INIT_LIST_HEAD(&defcls.list);

	defcls.clsid = defid;
	defcls.rate = rate;
	if(ceil) {
		defcls.ceil = ceil;
	} else {
		defcls.ceil = rate;
	}
	defcls.gurantee = TOTAL_RATE;

	defcls.pinfos = malloc(sizeof(struct pinfo));
	INIT_LIST_HEAD(&defcls.pinfos->list);
}

struct clsinfo* clsinfo_create_cls(__u32 clsid, __u64 rate, __u64 ceil, __u64 gurantee) 
{
	struct clsinfo* tmp = malloc(sizeof(struct clsinfo));
	tmp->clsid = clsid;
	tmp->rate = rate;
	if(ceil) {
		tmp->ceil = ceil;
	} else {
		tmp->ceil = rate;
	}

	tmp->gurantee = gurantee;
	if(defcls.gurantee - tmp->gurantee < 0) {
		free(tmp);
		return NULL;
	} else {
		defcls.gurantee -= tmp->gurantee;
	}

	tmp->pinfos = malloc(sizeof(struct pinfo));
	INIT_LIST_HEAD(&tmp->pinfos->list);	
	list_add(&tmp->list, &defcls.list);	

	return tmp;
}

void clsinfo_destroy_cls(struct clsinfo* clsinfo) 
{
	struct pinfo* pos;
	struct pinfo* n;
	struct list_head* head = &clsinfo->pinfos->list;

	/* pos: data type * 
	* n: next data type *
	* head: list of data type
	* member: member name of list in data type
	***/
	list_for_each_entry_safe(pos, n, head, list) {
		list_del(&pos->list);				
		free(pos);
	}

	free(clsinfo);
}

int clsinfo_add_pid(__u32 clsid, char* pid) 
{
	struct clsinfo* target = NULL;
	struct clsinfo* pos = NULL;
	struct list_head* head = &pos->pinfos->list;
	list_for_each_entry(pos, head, list) {
		if(pos->clsid == clsid) {
			target = pos;
			break;
		}
	}

//	if(target == NULL) {
//		target = clsinfo_create_cls(clsid, rate, ceil, gurantee);
//		if(target == NULL) {	/* Not enough gurantee */
//			return -1;
//		}
//	} else {
//		target->gurantee += gurantee;
//	}

	struct pinfo* pinfo = malloc(sizeof(struct pinfo));
	memset(pinfo->pid, 0, 8);
	strncpy(pinfo->pid, pid, strlen(pid));

	list_add(&pinfo->list, &target->pinfos->list);

	return defcls.gurantee;
}

int clsinfo_add(__u32 clsid, char* pid, __u32 rate, __u32 ceil, __u32 gurantee) 
{
	struct clsinfo* target = NULL;
	struct clsinfo* pos = NULL;
	struct list_head* head = &pos->pinfos->list;
	list_for_each_entry(pos, head, list) {
		if(pos->clsid == clsid) {
			target = pos;
			break;
		}
	}

	if(target == NULL) {
		target = clsinfo_create_cls(clsid, rate, ceil, gurantee);
		if(target == NULL) {	/* Not enough gurantee */
			return -1;
		}
	} else {
		target->gurantee += gurantee;
	}

	struct pinfo* pinfo = malloc(sizeof(struct pinfo));
	memset(pinfo->pid, 0, 8);
	strncpy(pinfo->pid, pid, strlen(pid));
	pinfo->gurantee = gurantee;

	list_add(&pinfo->list, &target->pinfos->list);

	return defcls.gurantee;
}
