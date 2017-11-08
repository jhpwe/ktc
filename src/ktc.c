#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <mqueue.h>
#include <time.h>
#include <pthread.h>

#include "gcls.h"
#include "utils.h"
#include "ktc_tc.h"
#include "list.h"

#define KTC_MQ_PATH "/ktc_mq"

char start_path[128];

int d_quit;
pthread_mutex_t p_mutex = PTHREAD_MUTEX_INITIALIZER;

mqd_t msgq_init(struct mq_attr* attr)
{
	mqd_t mfd;

	attr->mq_maxmsg = 10;
	attr->mq_msgsize = sizeof(struct ktc_mq_s);

	mfd = mq_open(KTC_MQ_PATH, O_RDWR | O_CREAT,  0666, attr);
	if (mfd == -1)
	{
		return -1;
	}
	return mfd;
}

int msgq_release(mqd_t mfd)
{
	if(mq_close(mfd) < 0) {
		ktclog(start_path, NULL, "mq_close error");
		return -1;
	}

	remove("/dev/mqueue/ktc_mq");

	return 0;
}

int msgq_get(mqd_t mfd, struct mq_attr* attr, struct ktc_mq_s* kmq)
{
	if((mq_receive(mfd, (char *)kmq, attr->mq_msgsize,NULL)) == -1)
	{
		return -1;
	}
	else
	{
		char tmp[128];
		memset(tmp, 0, 128);
		sprintf(tmp, "received : %s %s",kmq->cmd, kmq->pid);
		ktclog(start_path, NULL, tmp);
	}
	return 0;
}

int ktc_proc_insert(char* pid, char* low, char* high)
{
	if(check_pid(pid))
	{
		ktclog(start_path, NULL, "process is not running");
		return -1;
	}

	if((gcls_add(pid, low, high)) != 0)
	{
		//printf("total rate over the max link speed\n");
		return -1;
	}

	return 0;
}

int ktc_proc_change(char* pid, char* low, char* high)
{
	if(check_pid(pid))
	{
		ktclog(start_path, NULL, "process is not running");
		return -1;
	}

	if((gcls_modify(pid, low, high)) != 0)
	{
		ktclog(start_path, NULL, "gcls modify failed");
		return -1;
	}

	return 0;
}

int ktc_proc_delete(char* pid)
{
	if(check_pid(pid))
	{
		ktclog(start_path, NULL, "process is not running");
	//	return -1;
	}

	if((gcls_delete_pid(pid)) != 0) //fail < 0, success = res gurantee
	{
		ktclog(start_path, NULL, "gcls delete failed");
		return -1;
	}

	return 0;
}

void usage(void)
{
	printf("Usage: ktc dev [DEVICE NAME] link [MAX LINK SPEED]\n");
}

char dev[16] = {};
char link_speed[16] = {};

int monitor() {
	struct gcls* target = NULL;
	struct gcls* pos = NULL;
	struct gcls* next = NULL;
	struct list_head* head = gcls_get_head();
	char tmp[128];

	while(d_quit) {
		// lock
		pthread_mutex_lock(&p_mutex);

		/* in loop, check absence of pid */
		list_for_each_entry_safe(pos, next, head, list) {
			if(check_pid(pos->pid)) {	/* if absence, delete */
				memset(tmp, 0, 128);
				sprintf(tmp, "before %s is deleted check_pid result: %llu", pos->pid, gcls_get_remain());
				ktclog(start_path, NULL, tmp);

				cgroup_proc_del(pos->pid);
				gcls_delete_pid(pos->pid);
				memset(tmp, 0, 128);
				sprintf(tmp, "after %s is deleted check_pid result: %llu", pos->pid, gcls_get_remain());
			
				ktclog(start_path, NULL, tmp);
			}
		}

		//ktclog(start_path, NULL, "here");
		/* modify & updates all cls_show */
		cls_show(dev); /* updates class info, rate(low) & ceil(high) of list */
		list_for_each_entry_safe(pos, next, head, list) {
			if(pos->mod == 0) {
				cgroup_proc_del(pos->pid);
				gcls_delete_pid(pos->pid);	/* if not modified == not exist in real tc --> delete */
				memset(tmp, 0, 128);
				sprintf(tmp, "%s is deleted not modified", pos->pid);
			
				ktclog(start_path, NULL, tmp);
			} else {
				pos->mod = 0;
			}
		}
		//ktclog(start_path, NULL, "here2");
		
//		memset(tmp, 0, 128);
//		sprintf(tmp, "current remain %d", gcls_get_remain());
//		ktclog(start_path, NULL, tmp);
		/* set remain rate to default class */
		cls_modify(dev, 0x010001, 0x010002, 0, link_speed, KTC_CHANGE_DEFUALT, gcls_get_remain());

		//unlock
		pthread_mutex_unlock(&p_mutex);

		sleep(1);
	}
}

void* msg_handler(void* data)
{
	mqd_t mfd;
	struct mq_attr attr;
	struct ktc_mq_s kmq;

	if((mfd = msgq_init(&attr)) < 0)
	{
		ktclog(start_path, NULL, "msgq open error");
		exit(-1);
	}

	while(1)
	{
		msgq_get(mfd, &attr, &kmq);

		pthread_mutex_lock(&p_mutex);

		if(strcmp(kmq.cmd, "add") == 0)
		{
				ktc_proc_insert(kmq.pid, kmq.lower, kmq.upper);
				ktclog(start_path, &kmq, NULL);
		}
		else if(strcmp(kmq.cmd, "change") == 0)
		{
				ktc_proc_change(kmq.pid, kmq.lower, kmq.upper);
				ktclog(start_path, &kmq, NULL);
		}
		else if(strcmp(kmq.cmd, "delete") == 0)
		{
				ktc_proc_delete(kmq.pid);
				ktclog(start_path, &kmq, NULL);
		}
		else if(strcmp(kmq.cmd, "quit") == 0)
		{
				ktclog(start_path, &kmq, NULL);
				d_quit = 0;
				break;
		}
		else if(strcmp(kmq.cmd, "show") == 0)
		{
				gcls_show();
		}
		pthread_mutex_unlock(&p_mutex);
	}
	msgq_release(mfd);

	exit(0);
}

int main(int argc, char** argv)
{
	if(access("/", R_OK | W_OK) != 0) {
		printf("Must run as root.\n");
		return -1;
	}

	if(argc != 5)
	{
		usage();
		return -1;
	}

	argc--;
	argv++;

	while(argc > 0)
	{
		if(strcmp(*argv, "dev") == 0)
		{
			argc--;
			argv++;
			strncpy(dev, *argv, sizeof(dev)-1);
		}
		else if(strcmp(*argv, "link") == 0)
		{
			argc--;
			argv++;
			strncpy(link_speed, *argv, sizeof(dev)-1);
		}
		else
		{
			printf("Unknown argument : %s\n", *argv);
			return -1;
		}
		argc--;
		argv++;
	}

	getcwd(start_path, 128);
	strcat(start_path, "/ktclog.txt");
	printf("%s\n", start_path);

	/* daemon create start */
	int pid = fork();
	if(pid < 0) {
		ktclog(start_path, NULL, "fork failed");
		exit(0);
	} else if(pid > 0) {	/* Parent kill */
		ktclog(start_path, NULL, "parent dead");
		exit(0);
	} else {
		ktclog(start_path, NULL, "daemon start");
	}

	signal(SIGHUP, SIG_IGN);	/* Ignore SIGHUP(Terminal disconnected) */
	close(0);	/* close stderr, stdin, stdout */
	close(1);
	close(2);

	chdir("/");	/* Move to root */
	setsid(); /* Make session reader */
	/* daemon end */

	cgroup_init();
	qdisc_init(dev, 0x010000, 0x2);

	/* root class */
	cls_modify(dev, 0x010000, 0x010001, link_speed, link_speed, KTC_CREATE_CLASS, 0);
	/* default class */
	cls_modify(dev, 0x010001, 0x010002, link_speed, link_speed, KTC_CREATE_CLASS, 0);

	filter_add(dev, 0x010000, "10", "1:");
	gcls_init(0x010000, 0x010002, link_speed);

	ktclog(start_path, NULL, "initialization done");

	//msg_loop(mfd, &kmq, &attr, dev, link_speed); --> TODO: Thread function
	pthread_t thread_id;
	int status;

	d_quit = 1;

	pthread_create(&thread_id, NULL, &msg_handler, NULL);

	//monitor
	monitor();

	pthread_join(thread_id, (void **)&status);

	pthread_mutex_destroy(&p_mutex);

	ktclog(start_path, NULL, "daemon exit");

	return 0;
}
