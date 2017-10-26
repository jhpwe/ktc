#ifndef __GCLS_H__
#define __GCLS_H__

#include <list.h>
#include <sys/types.h>
#include <asm/types.h>

struct gcls {
	struct list_head	list;
	__u32				id;
	char				pid[8];

	__u64				low;
	__u64				high;

	int					mod;
};

void gcls_show();
struct list_head* gcls_get_head();
__u64 gcls_get_remain();
void gcls_init(__u32 parent, __u32 defid, char* link_max);
__u32 gcls_check_pid(char* pid);
int gcls_check_classid(__u32 cid);
__u32 gcls_empty_id();
int gcls_add(char* pid, char* clow, char* chigh);
int gcls_modify(char* pid, char* clow, char* chigh);
int gcls_modify_u(__u32 clsid, __u64 rate, __u64 ceil);
int gcls_delete_pid(char* pid);

#endif /* __GCLS_H__ */
