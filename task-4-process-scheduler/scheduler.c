#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

#include <sys/wait.h>
#include <sys/types.h>

#include "proc-common.h"
#include "request.h"

/* Compile-time parameters. */
#define SCHED_TQ_SEC 10               /* time quantum */
#define TASK_NAME_SZ 60               /* maximum size for a task's name */

struct process_node {
    pid_t					pid;
   	int						id;
    struct process_node	*next;
};
struct process_node *head = NULL, *tail = NULL;

/* SIGALRM handler: Gets called whenever an alarm goes off.
 * The time quantum of the currently executing process has expired,
 * so send it a SIGSTOP. The SIGCHLD handler will take care of
 * activating the next in line.
 */
static void
sigalrm_handler(int signum)
{
	if (head != tail) kill(head -> pid, SIGSTOP);
}

/* SIGCHLD handler: Gets called whenever a process is stopped,
 * terminated due to a signal, or exits gracefully.
 *
 * If the currently executing task has been stopped,
 * it means its time quantum has expired and a new one has
 * to be activated.
 */
static void
sigchld_handler(int signum)
{
	struct process_node *nxt = NULL, *prv = NULL;
	int status;
	pid_t p;
	do
	{
		p = waitpid(-1, &status, WUNTRACED | WNOHANG);
		if (p < 0)
		{
			perror("waitpid");
			exit(1);
		}

		if (p > 0)
		{
			explain_wait_status(p, status);
			if (WIFEXITED(status) || WIFSIGNALED(status))
			{
				nxt = head;
				if (p == head -> pid)
				{
					head = head -> next;
					free(nxt);
					if (head == NULL)
					{
						printf("No tasks left. Exiting...\n");
						exit(0);
					}
					alarm(SCHED_TQ_SEC);
				}
				else
				{
					kill(head -> pid, SIGSTOP);
					while (p != nxt -> pid)
					{
						prv = nxt;
						nxt = nxt -> next;
					}
					prv -> next = nxt -> next;
					if (prv -> next == NULL) tail = prv;
					free(nxt);
				}
			}
			if (WIFSTOPPED(status))
			{
				tail -> next = head;
				tail = head;
				head = head -> next;
				tail -> next = NULL;
				alarm(SCHED_TQ_SEC);
			}
			kill(head -> pid, SIGCONT);
		}
	} while (p > 0);
}

/* Install two signal handlers.
 * One for SIGCHLD, one for SIGALRM.
 * Make sure both signals are masked when one of them is running.
 */
static void
install_signal_handlers(void)
{
	sigset_t sigset;
	struct sigaction sa;

	sa.sa_handler = sigchld_handler;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGCHLD);
	sigaddset(&sigset, SIGALRM);
	sa.sa_mask = sigset;
	if (sigaction(SIGCHLD, &sa, NULL) < 0) {
		perror("sigaction: sigchld");
		exit(1);
	}

	sa.sa_handler = sigalrm_handler;
	if (sigaction(SIGALRM, &sa, NULL) < 0) {
		perror("sigaction: sigalrm");
		exit(1);
	}

	/*
	 * Ignore SIGPIPE, so that write()s to pipes
	 * with no reader do not result in us being killed,
	 * and write() returns EPIPE instead.
	 */
	if (signal(SIGPIPE, SIG_IGN) < 0) {
		perror("signal: sigpipe");
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	int i, nproc = argc - 1;
	pid_t p;
	struct process_node *n;
	char executable[10];
	char *newargv[] = { executable, NULL, NULL, NULL };
	char *newenviron[] = { NULL };
	
	/*
	 * For each of argv[1] to argv[argc - 1],
	 * create a new child process, add it to the process list.
	 */
    
	for (i = 1; i <= nproc; i++)
	{
		p = fork();
		if (p < 0)
		{				/*Error*/
			perror("fork_procs: fork");
			exit(1);
		}
		if (p == 0)
		{
			printf("%ld: Raising SIGSTOP...\n", (long)getpid());
			raise(SIGSTOP);
			execve(argv[i], newargv, newenviron);
			/* execve() only returns on error */
			perror("execve");
			exit(1);
		}
		n = (struct process_node *) malloc(sizeof(struct process_node));
		n -> pid = p;
		n -> id  = i;
		if (head == NULL) {
			head = n;
			tail = n;
		}
		else {
			tail -> next = n;
			tail = n;
		}
	}

	/* Wait for all children to raise SIGSTOP before exec()ing. */
	wait_for_ready_children(nproc);

	/* Install SIGALRM and SIGCHLD handlers. */
	install_signal_handlers();

	if (nproc == 0)
	{
		fprintf(stderr, "Scheduler: No tasks. Exiting...\n");
		exit(1);
	}

	show_pstree(getpid());	
	alarm(SCHED_TQ_SEC);
	kill(head -> pid, SIGCONT);
	
	/* loop forever  until we exit from inside a signal handler. */
	while (pause())
		;
	
	/* Unreachable */
	fprintf(stderr, "Internal error: Reached unreachable point\n");
	return 1;
}
