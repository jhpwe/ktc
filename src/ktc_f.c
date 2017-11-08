#include <mqueue.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#define KTC_MQ_PATH "/ktc_mq"

struct ktc_mq_s
{
  char cmd[8];
  char pid[8];
  char upper[16];
  char lower[16];
  char recvmsg[32];
};

void usage(void)
{
	printf("Usage: ktc_f [commands] pid [pid] upper [Xmbit] lower [Xmbit]\n[commands] : add, delete, change\n");
}

int main(int argc, char **argv)
{
    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(struct ktc_mq_s);
    struct ktc_mq_s kmq = {};

    mqd_t mfd;

    mfd = mq_open(KTC_MQ_PATH, O_RDWR, 0666, &attr);
    if(mfd == -1)
    {
        perror("msgq open error");
        return -1;
    }

    if(argc < 2)
    {
      usage();
      return -1;
    }

    argc--;
    argv++;

  	while(argc > 0)
  	{
      if(strcmp(*argv, "add") == 0)
  		{
        if(kmq.cmd[0] != 0)
        {
          printf("Duplicate command.\n");
          usage();
          return -1;
        }
        strncpy(kmq.cmd, *argv, sizeof(kmq.cmd)-1);
  		}
      else if(strcmp(*argv, "change") == 0)
  		{
        if(kmq.cmd[0] != 0)
        {
          printf("Duplicate command.\n");
          usage();
          return -1;
        }
        strncpy(kmq.cmd, *argv, sizeof(kmq.cmd)-1);
  		}
      else if(strcmp(*argv, "delete") == 0)
  		{
        if(kmq.cmd[0] != 0)
        {
          printf("Duplicate command.\n");
          usage();
          return -1;
        }
        strncpy(kmq.cmd, *argv, sizeof(kmq.cmd)-1);
  		}
      else if(strcmp(*argv, "quit") == 0)
  		{
        if(kmq.cmd[0] != 0)
        {
          printf("Duplicate command.\n");
          usage();
          return -1;
        }
        strncpy(kmq.cmd, *argv, sizeof(kmq.cmd)-1);
  		}
	else if(strcmp(*argv, "show") == 0)
  		{
        if(kmq.cmd[0] != 0)
        {
          printf("Duplicate command.\n");
          usage();
          return -1;
        }
        strncpy(kmq.cmd, *argv, sizeof(kmq.cmd)-1);
  		}
  		else if(strcmp(*argv, "pid") == 0)
  		{
  			argc--;
  			argv++;
        if(argc > 0)
  			   strncpy(kmq.pid, *argv, sizeof(kmq.pid)-1);
        else
          usage();
  		}
  		else if(strcmp(*argv, "upper") == 0)
  		{
  			argc--;
  			argv++;
        if(argc > 0)
  			   strncpy(kmq.upper, *argv, sizeof(kmq.upper)-1);
       else
         usage();
  		}
      else if(strcmp(*argv, "lower") == 0)
      {
        argc--;
        argv++;
        if(argc > 0)
          strncpy(kmq.lower, *argv, sizeof(kmq.lower)-1);
        else
          usage();
      }
  		else
  		{
  			printf("Unknown argument : %s\n", *argv);
  			usage();
  			return -1;
  		}
  		argc--;
  		argv++;
  	}

    if( (strcmp(kmq.cmd, "add") && strcmp(kmq.cmd, "delete") && strcmp(kmq.cmd, "change") && strcmp(kmq.cmd, "quit") && strcmp(kmq.cmd, "show")) )
    {
      printf("Unknown Command %s\n", kmq.cmd);
      usage();
      return -1;
    }

    if(strcmp(kmq.cmd, "quit") != 0 && strcmp(kmq.cmd, "show") != 0 && strcmp(kmq.cmd, "delete") != 0 )
    {
      if(kmq.pid[0] == 0 || kmq.lower[0] == 0)
      {
        usage();
        return -1;
      }
    }

	if(strcmp(kmq.cmd, "delete") == 0)
	{
		if(kmq.pid[0] == 0)
		{
			usage();
			return -1;
		}
	}

    //no ceil
    strncpy(kmq.upper, kmq.lower, sizeof(kmq.lower));

    if((mq_send(mfd, (char *)&kmq, attr.mq_msgsize, 1)) == -1)
    {
        perror("send error");
        return -1;
    }

/*
    if((mq_receive(mfd, (char *)&kmq, attr.mq_msgsize, NULL)) == -1)
    {
      perror("receive error");
      return -1;
    }

    printf("%s", kmq.recvmsg);
*/

    mq_close(mfd);

    return 0;
}
