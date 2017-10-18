#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <list.h>
#include <utils.h>
#include "gurantee.h"

#define CLS_MAX	100

static __u32 clsids[CLS_MAX];
static __u32 TOTAL_RATE = 1000000000; // 1Gbit/sec
static struct clsinfo defcls;
static __u64 basic_total;

void clsinfo_show() 
{
	struct clsinfo* cls_pos;
	struct list_head* cls_head = &defcls.list;
	int i = 0;

	printf("[DEFAULT]");
	printf("id:0x%X / rate:%lld / ceil: %lld / gurantee: %lld\n", 
				defcls.clsid, defcls.rate, defcls.ceil, defcls.gurantee);

	struct pinfo* p_pos;
	struct list_head* p_head = &defcls.pinfos->list;

	list_for_each_entry(p_pos, p_head, list) {
		printf("\tpid: 0x%s / gurantee: %lld\n", p_pos->pid, p_pos->gurantee);
	}

	list_for_each_entry(cls_pos, cls_head, list) {
		printf("id:0x%X / rate:% lld / ceil: %lld / gurantee: %lld\n", 
				cls_pos->clsid, cls_pos->rate, cls_pos->ceil, cls_pos->gurantee);

		p_pos = NULL;
		p_head = &cls_pos->pinfos->list;

		list_for_each_entry(p_pos, p_head, list) {
			printf("\tpid: 0x%s / gurantee: 0x%lld\n", p_pos->pid, p_pos->gurantee);
		}
	}
}

void clsinfo_init(__u32 parent, __u32 defid, char* total, __u32 rate, __u32 ceil) 
{
	INIT_LIST_HEAD(&defcls.list);

	defcls.clsid = defid | parent;
	get_rate64(&basic_total, total);
	get_rate64(&defcls.rate, total);
	get_rate64(&defcls.ceil, total);
	get_rate64(&defcls.gurantee, total);

	for(int i = 0; i < CLS_MAX; i++)
		clsids[i] = 0;
	clsids[0] = clsids[1] = 1;

	defcls.pinfos = malloc(sizeof(struct pinfo));
	INIT_LIST_HEAD(&defcls.list);
	INIT_LIST_HEAD(&defcls.pinfos->list);
}

struct clsinfo* clsinfo_create_cls(__u32 clsid, __u64 rate, __u64 ceil, __u64 gurantee) 
{
	if((clsid & 0xff) == defcls.clsid) {
		defcls.rate = rate;
		defcls.ceil = ceil;
		defcls.gurantee = gurantee;
		return &defcls;
	} else {
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
		p_pos = NULL;
		p_head = &cls_pos->pinfos->list;
{
	struct clsinfo* target = NULL;
	struct clsinfo* pos = NULL;

	if((clsid & 0xff) == defcls.clsid) {
		target = &defcls;
	} else {
		struct list_head* head = &defcls.list;
		list_for_each_entry(pos, head, list) {
			if(pos->clsid == clsid) {
				target = pos;
				break;
			}
		}
	}

	if(!target) {
		fprintf(stderr, "Cant find pid 0x%X\n", clsid);
		return -1;
	}

	struct pinfo* pinfo = malloc(sizeof(struct pinfo));
	memset(pinfo->pid, 0, 8);
	strncpy(pinfo->pid, pid, strlen(pid));
	pinfo->gurantee = target->gurantee;

	list_add(&pinfo->list, &target->pinfos->list);

	return defcls.gurantee;
}

int clsinfo_del_pid(char* pid) {
	struct pinfo* p_pos = NULL;
	struct pinfo* n = NULL;
	struct list_head* p_head = &defcls.pinfos->list;
	list_for_each_entry_safe(p_pos, n, p_head, list) {
		if(strcmp(p_pos->pid, pid) == 0) {
			printf("\t[delete] pid: 0x%s from %x\n", p_pos->pid, defcls.clsid);
			list_del(&p_pos->list);
			break;
		}
	}

	if(p_pos) {
		free(p_pos);
		return 0;
	}

	struct clsinfo* cls_pos;
	struct clsinfo* pos = NULL;
	struct list_head* cls_head = &defcls.list;

	list_for_each_entry(cls_pos, cls_head, list) {
		printf("id:0x%X / rate:0x%llx / ceil: 0x%llx / gurantee: 0x%llx\n", 
				cls_pos->clsid, cls_pos->rate, cls_pos->ceil, cls_pos->gurantee);

		p_pos = NULL;
		p_head = &cls_pos->pinfos->list;

		list_for_each_entry(p_pos, p_head, list) {
			printf("\tpid: 0x%s / gurantee: 0x%llx\n", p_pos->pid, p_pos->gurantee);

			if(strcmp(p_pos->pid, pid) == 0) {
				printf("\t[delete] pid: 0x%s from %x\n", p_pos->pid, cls_pos->clsid);
				list_del(&p_pos->list);
				break;
			}
		}
	}
	
	if(p_pos) {
		free(p_pos);
		return 0;
	}

	return 1;	
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

__u32 clsinfo_check_pid(char* pid) {
	struct clsinfo* cls_pos;
	struct list_head* cls_head;
	struct pinfo* p_pos;
	struct list_head* p_head;

	p_pos = NULL;
	p_head = &defcls.pinfos->list;
	list_for_each_entry(p_pos, p_head, list) {
		if(strcmp(p_pos->pid, pid) == 0) {
			return defcls.clsid;			
			break;
		}
	}

	p_pos = NULL;
	cls_pos = NULL;
	cls_head = &defcls.list;
	printf("target: %s\n", pid);
	list_for_each_entry(cls_pos, cls_head, list) {
		printf("clsid: 0x%x / pid: 0x%s\n", cls_pos->clsid, cls_pos->pinfos->pid);
		if(strcmp(cls_pos->pinfos->pid, pid) == 0) {
			return cls_pos->clsid;
		}
	}

	return 0;
}

__u32 clsinfo_create_clsid(void) {
	int i;
	for(i = 1; i < CLS_MAX; i++) {
		if(clsids[i] == 0) {
			clsids[i] = 1;
			break;
		}
	}

	return (defcls.clsid & 0xff0000) | i;
}

__u64 clsinfo_pid_add(char* pid, __u32 clsid, char* rate, char* ceil, char* gurantee) {
	__u64 g;
	get_rate64(&g, gurantee);
	if(g >= defcls.gurantee) 
		return -1;

	struct clsinfo* tmp = malloc(sizeof(struct clsinfo));

	tmp->clsid = clsid;
	get_rate64(&tmp->rate, rate);
	get_rate64(&tmp->ceil, ceil);
	tmp->gurantee = g;

	list_add(&tmp->list, &defcls.list);	

	struct pinfo* pinfo = malloc(sizeof(struct pinfo));
	memset(pinfo->pid, 0, 8);
	strncpy(pinfo->pid, pid, strlen(pid));
	pinfo->gurantee = tmp->gurantee;

	tmp->pinfos = pinfo;
	INIT_LIST_HEAD(&tmp->pinfos->list);

	defcls.gurantee -= tmp->gurantee;

	return defcls.gurantee;	
}

__u64 clsinfo_pid_change(char* pid, __u32 clsid, char* rate, char* ceil, char* gurantee) {
	struct clsinfo* cls_target = NULL;
	struct pinfo* p_target = NULL;
	struct clsinfo* cls_pos;
	struct list_head* cls_head;
	struct pinfo* p_pos;
	struct list_head* p_head;

	p_pos = NULL;
	p_head = &defcls.pinfos->list;
	list_for_each_entry(p_pos, p_head, list) {
		if(strcmp(p_pos->pid, pid) == 0) {
			cls_target = &defcls;
			p_target = p_pos;
			break;
		}
	}

	p_pos = NULL;
	cls_pos = NULL;
	cls_head = &defcls.list;
	list_for_each_entry(cls_pos, cls_head, list) {
		if(clsid == cls_pos->clsid) {
			p_pos = NULL;
			p_head = &cls_pos->pinfos->list;
			cls_target = cls_pos;
			list_for_each_entry(p_pos, p_head, list) {
				if(strcmp(p_pos->pid, pid) == 0) {
					p_target = p_pos;
					break;
				}
			}	
			
			cls_target = cls_pos;
			break;
		}
	}

	if(!cls_target)
		return 0;

	__u64 g, r, c;
	get_rate64(&g, gurantee);
	get_rate64(&r, rate);
	get_rate64(&c, ceil);

	if(g > cls_pos->gurantee) {
		if((defcls.gurantee - (g - cls_pos->gurantee)) < 0) {
			return -1;
		}	

		defcls.gurantee -= (g - cls_pos->gurantee);
	} else {
		defcls.gurantee += (cls_pos->gurantee - g);
		if(defcls.gurantee > basic_total) {
			defcls.gurantee = basic_total;
		}
	}

	cls_pos->gurantee = g;
	cls_pos->ceil = c;
	cls_pos->rate = r;

	return defcls.gurantee;
}

__u64 clsinfo_pid_delete(char* pid, __u32 clsid) {
	__u64 remain = 0;
	struct pinfo* p_pos = NULL;
	struct pinfo* n = NULL;
	struct list_head* p_head = &defcls.pinfos->list;

	if(clsid == defcls.clsid) {
		list_for_each_entry_safe(p_pos, n, p_head, list) {
			if(strcmp(p_pos->pid, pid) == 0) {
				printf("\t[delete] pid: 0x%s from %x\n", p_pos->pid, defcls.clsid);
				list_del(&p_pos->list);
				break;
			}
		}

		if(p_pos) {
			defcls.gurantee += p_pos->gurantee;
			free(p_pos);
			return defcls.gurantee;
		}
	}	

	struct clsinfo* cls_pos = NULL;
	struct clsinfo* target = NULL;
	struct clsinfo* cls_n = NULL;
	struct list_head* cls_head = &defcls.list;

	list_for_each_entry_safe(cls_pos, cls_n, cls_head, list) {
		if(cls_pos->clsid == clsid) {
			p_pos = NULL;
			p_head = &cls_pos->pinfos->list;

			list_for_each_entry(p_pos, p_head, list) {
				if(strcmp(p_pos->pid, pid) == 0) {
					printf("\t[delete] pid: 0x%s from %x\n", p_pos->pid, cls_pos->clsid);
					list_del(&p_pos->list);
					break;
				}
			}

			list_del(&cls_pos->list);
			target = cls_pos;
			break;	
		}
	}
	
	if(!target)
		return -1;
	
	defcls.gurantee += target->gurantee;	

	free(target);

	return defcls.gurantee;	
}
