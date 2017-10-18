#ifndef __GCLS_H__
#define __GCLS_H__

#include <list.h>
#include <sys/types.h>
#include <asm/types.h>

void gcls_show();
void gcls_init(__u32 parent, __u32 defid, char* link_max);
__u32 gcls_check_pid(char* pid);
__u32 gcls_empty_id();
__u64 gcls_add(__u32 clsid, char* pid, char* clow, char* chigh);
__u64 gcls_delete_pid(char* pid);

#endif /* __GCLS_H__ */
