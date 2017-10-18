CC = gcc
INCDIR	= ./inc
SRCDIR	= ./src
OBJDIR	= ./obj
OBJS = $(OBJDIR)/libnetlink.o $(OBJDIR)/ll_map.o $(OBJDIR)/ll_proto.o $(OBJDIR)/ktc.o $(OBJDIR)/utils.o $(OBJDIR)/tc_core.o $(OBJDIR)/gurantee.o $(OBJDIR)/gcls.o
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c -o $@ $< -I$(INCDIR)

ktc: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

all: ktc

clean:
	rm -rf $(OBJDIR)/*
	rm -rf ktc
