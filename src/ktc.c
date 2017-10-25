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

#include "gcls.h"
#include "utils.h"
#include "ktc_tc.h"

#define KTC_MQ_PATH "/ktc_mq"

char start_path[128];

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
		sprintf(tmp, "received : %s %s\n",kmq->cmd, kmq->pid);
		ktclog(start_path, NULL, tmp);
	}
	return 0;
}

void usage(void)
{
	printf("Usage: ktc dev [DEVICE NAME] link [MAX LINK SPEED]\n");
}

int main(int argc, char** argv)
{
	char dev[16] = {};
	char link_speed[16] = {};

	mqd_t mfd;
	struct mq_attr attr;
	struct ktc_mq_s kmq;

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

	if((mfd = msgq_init(&attr)) < 0)
	{
		ktclog(start_path, NULL, "msgq open error");
		return -1;
	}

	while(1)
	{
		if(msgq_get(mfd, &attr, &kmq) < 0)
		{
			ktclog(start_path, NULL, "msgq get error");
			return -1;
		}

		ktclog(start_path, &kmq, NULL);
		if(strcmp(kmq.cmd, "add") == 0)
		{
			ktc_proc_insert(dev, kmq.pid, kmq.lower, kmq.upper, link_speed);
		}
		else if(strcmp(kmq.cmd, "change") == 0)
		{
			ktc_proc_change(dev, kmq.pid, kmq.lower, kmq.upper, link_speed);
		}
		else if(strcmp(kmq.cmd, "delete") == 0)
		{
			ktc_proc_delete(dev, kmq.pid, link_speed);
		} 
		else if(strcmp(kmq.cmd, "quit") == 0) 
		{
			break;
		}
	}

	msgq_release(mfd);
	ktclog(start_path, NULL, "daemon exit");

	return 0;
}
