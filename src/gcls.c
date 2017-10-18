#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "utils.h"
#include "gcls.h"

#define CLSMAX 	0xFF

struct defgcls {
	struct list_head	list;
	__u32				id;	
	int					size;
	
	__u64				link;
	__u64				low;
	__u64				high;
};

struct gcls {
	struct list_head	list;
	__u32				id;
	char				pid[8];

	__u64				low;
	__u64				high;
};

static struct defgcls 	defgcls;
static __u32 			root;
static __u32			empty[CLSMAX];

void gcls_show() {
	struct gcls* pos = NULL;
	struct list_head* head = &defgcls.list;

	printf("[default] id: 0x%x / link_speed: %lld / low: %lld / high: %lld\n", defgcls.id, defgcls.link, defgcls.low, defgcls.high);
	int i = 0;
	list_for_each_entry(pos, head, list) {
		printf("[%d] id: 0x%x / pid: 0x%s / low: %lld / high: %lld\n", i++, pos->id, pos->pid, pos->low, pos->high);
	}	
}

void gcls_init(__u32 parent, __u32 defid, char* link_max) {
	root = parent;
	defgcls.id = defid;
	get_rate64(&defgcls.link, link_max);
	get_rate64(&defgcls.low, link_max);
	get_rate64(&defgcls.high, link_max);
	INIT_LIST_HEAD(&defgcls.list);

	empty[0] = 1;
	empty[defid & 0xff] = 1;
	for(int i = 1; i < CLSMAX; i++)
		empty[i] = 0;
}

__u32 gcls_empty_id() {
	for(int i = 0; i < CLSMAX; i++) {
		if(empty[i] == 0) {
			empty[i] = 1;
			return root | i;
		}
	}
}

struct gcls* gcls_create(__u32 clsid, char* pid, __u64 low, __u64 high) {
	struct gcls* tmp;
	if(low > defgcls.low)
		return NULL;

	tmp = malloc(sizeof(struct gcls));
	tmp->id = clsid;
	memset(tmp->pid, 0, 8);	
	strncpy(tmp->pid, pid, strlen(pid));
	tmp->low = low;
	tmp->high = high;

	defgcls.low -= tmp->low;

	return tmp;
}

struct gcls* gcls_get_id(__u32 target_id) {
	struct gcls* target = NULL;
	struct gcls* pos = NULL;
	struct list_head* head = &defgcls.list;

	list_for_each_entry(pos, head, list) {
		if(target_id == pos->id) {
			target = pos;
			break;
		}
	}	

	return target;
}

struct gcls* gcls_get_pid(char* target_pid) {
	struct gcls* target = NULL;
	struct gcls* pos = NULL;
	struct list_head* head = &defgcls.list;

	list_for_each_entry(pos, head, list) {
		if(strncmp(pos->pid, target_pid, strlen(target_pid)) == 0) {
			target = pos;
			break;
		}
	}	

	return target;
}

__u32 gcls_check_pid(char* pid) {
	struct gcls* target = gcls_get_pid(pid);

	if(target)
		return target->id;
	else
		return 0;
}

__u64 gcls_add(__u32 clsid, char* pid, char* clow, char* chigh) {
	__u64 low, high;
	get_rate64(&low, clow);
	get_rate64(&high, chigh);

	struct gcls* new = gcls_create(clsid, pid, low, high);
	if(new == NULL)
		return ULONG_MAX;

	list_add(&new->list, &defgcls.list);	

	return defgcls.low;
}

__u64 gcls_modify(__u32 clsid, char* pid, __u64 low, __u64 high) {
	struct gcls* target = gcls_get_id(clsid);
	if(target == NULL)
		return ULONG_MAX;

	if((defgcls.low + target->low) < low) 
		return ULONG_MAX;

	defgcls.low += target->low;
	target->low = low;
	target->high = high;
	defgcls.low -= target->low;

	return defgcls.low;
}

__u64 gcls_delete_pid(char* pid) {
	struct gcls* del = gcls_get_pid(pid);	
	if(del == NULL)
		return ULONG_MAX;

	defgcls.low += del->low;
	list_del(&del->list);
	free(del);

	return defgcls.low;
}
