#
# Makefile
#
# Operating Systems, Exercise 4
#

CC = gcc
#CFLAGS = -Wall -g
CFLAGS = -Wall -O2 -g

all: scheduler scheduler-shell scheduler-shell-prio scheduler-prio-lab shell prog execve-example strace-test

scheduler: scheduler.o proc-common.o
	$(CC) -o scheduler scheduler.o proc-common.o

scheduler-shell: scheduler-shell.o proc-common.o
	$(CC) -o scheduler-shell scheduler-shell.o proc-common.o

scheduler-shell-prio: scheduler-shell-prio.o proc-common.o
	$(CC) -o scheduler-shell-prio scheduler-shell-prio.o proc-common.o

scheduler-prio-lab: scheduler-prio-lab.o proc-common.o
	$(CC) -o scheduler-prio-lab scheduler-prio-lab.o proc-common.o

shell: shell.o proc-common.o
	$(CC) -o shell shell.o proc-common.o

prog: prog.o proc-common.o
	$(CC) -o prog prog.o proc-common.o

execve-example: execve-example.o 
	$(CC) -o execve-example execve-example.o

strace-test: strace-test.o 
	$(CC) -o strace-test strace-test.o

proc-common.o: proc-common.c proc-common.h
	$(CC) $(CFLAGS) -o proc-common.o -c proc-common.c

shell.o: shell.c proc-common.h request.h
	$(CC) $(CFLAGS) -o shell.o -c shell.c

scheduler.o: scheduler.c proc-common.h request.h
	$(CC) $(CFLAGS) -o scheduler.o -c scheduler.c

scheduler-shell.o: scheduler-shell.c proc-common.h request.h
	$(CC) $(CFLAGS) -o scheduler-shell.o -c scheduler-shell.c

scheduler-shell-prio.o: scheduler-shell-prio.c proc-common.h request.h
	$(CC) $(CFLAGS) -o scheduler-shell-prio.o -c scheduler-shell-prio.c

scheduler-prio-lab.o: scheduler-prio-lab.c proc-common.h request.h
	$(CC) $(CFLAGS) -o scheduler-prio-lab.o -c scheduler-prio-lab.c

prog.o: prog.c
	$(CC) $(CFLAGS) -o prog.o -c prog.c

execve-example.o: execve-example.c
	$(CC) $(CFLAGS) -o execve-example.o -c execve-example.c

strace-test.o: strace-test.c
	$(CC) $(CFLAGS) -o strace-test.o -c strace-test.c

clean:
	rm -f scheduler scheduler-shell scheduler-shell-prio scheduler-prio-lab shell prog execve-example strace-test *.o
