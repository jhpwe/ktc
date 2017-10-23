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
#include "ktc_tc.h"

#define KTC_MQ_PATH "/ktc_mq"
#define KTC_LOG_PATH "/home/pjhubuntu/ktc"

struct ktc_mq_s
{
  char cmd[8];
  char pid[8];
  char upper[16];
  char lower[16];
};

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

int msgq_get(mqd_t mfd, struct mq_attr* attr, struct ktc_mq_s* kmq)
{
	if((mq_receive(mfd, (char *)kmq, attr->mq_msgsize,NULL)) == -1)
	{
		return -1;
	}
	else
	{
		//printf("received : %s %s\n",kmq->cmd, kmq->pid);
	}
	return 0;
}
void usage(void)
{
	//printf("Usage: ktc dev [DEVICE NAME] link [MAX LINK SPEED]\n");
}

int test_daemon_msg() {
	
	return 0;
}

int ktclog(struct ktc_mq_s* ktc_msg, char* comment) 
{
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	char path[128];
	char* home;
	FILE* logfile;

	home = getenv("HOME");
	memset(path, 0, 128);
	sprintf(path, "%s/ktc_log.txt", home);
	if((logfile = fopen("/home/pjhubuntu/ktc/log.txt", "a")) < 0 ) 
	{
		perror("log file ");	
		exit(0);
	}

	fprintf(logfile, "%d [%2d-%2d-%2d/%2d:%2d:%2d] ", getpid(), tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	if(ktc_msg) fprintf(logfile, "%s from %s bound(%s ~ %s) ", ktc_msg->cmd, ktc_msg->pid, ktc_msg->lower, ktc_msg->upper);
	if(comment)	fprintf(logfile, "%s", comment);
	fprintf(logfile, "\n");

	return 0;
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

	int pid = fork();
	if(pid < 0) {
		perror("fork error\n");
		exit(0);
	} else if(pid > 0) {	/* Parent kill */
		//printf("child[%d], parent[%d] kill\n", pid, getpid());
		exit(0);
	} else {
		//printf("process[%d] running\n", getpid());
	}

	char tmp[32];
	memset(tmp, 0, 32);
	sprintf(tmp, "ktc[%d] start", getpid());
	ktclog(NULL, tmp);

	signal(SIGHUP, SIG_IGN);	/* Ignore SIGHUP(Terminal disconnected) */
	close(0);	/* close stderr, stdin, stdout */
	close(1);
	close(2);

	chdir("/");	/* Move to root */
	setsid(); /* Make session reader */

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
			//printf("Unknown argument : %s\n", *argv);
			ktclog(NULL, "argv error");
			
			return -1;
		}
		argc--;
		argv++;
	}

	cgroup_init();
	qdisc_init(dev, 0x010000, 0x2);

	/* root class */
	cls_modify(dev, 0x010000, 0x010001, link_speed, link_speed, KTC_CREATE_CLASS, 0);
	/* default class */
	cls_modify(dev, 0x010001, 0x010002, link_speed, link_speed, KTC_CREATE_CLASS, 0);

	filter_add(dev, 0x010000, "10", "1:");
	gcls_init(0x010000, 0x010002, link_speed);

	if( (mfd = msgq_init(&attr)) < 0)
	{
		ktclog(NULL, "msgq open error");
		return -1;
	}


	while(1)
	{
		if(msgq_get(mfd, &attr, &kmq) < 0)
		{
			return -1;
		}

		if(strcmp(kmq.cmd, "add") == 0)
		{
			ktc_proc_insert(dev, kmq.pid, kmq.lower, kmq.upper, link_speed);
			ktclog(&kmq, NULL);
		}
		else if(strcmp(kmq.cmd, "change") == 0)
		{
			ktc_proc_change(dev, kmq.pid, kmq.lower, kmq.upper, link_speed);
			ktclog(&kmq, NULL);
		}
		else if(strcmp(kmq.cmd, "delete") == 0)
		{
			ktc_proc_delete(dev, kmq.pid, link_speed);
			ktclog(&kmq, NULL);
		} 
		else if(strcmp(kmq.cmd, "quit") == 0) 
		{
			ktclog(&kmq, NULL);
			exit(0);
		}
	}

	return 0;
}
