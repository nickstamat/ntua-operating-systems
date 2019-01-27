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
#define SCHED_TQ_SEC 7                /* time quantum */
#define TASK_NAME_SZ 60               /* maximum size for a task's name */
#define SHELL_EXECUTABLE_NAME "shell" /* executable for shell */

struct process_node {
    pid_t					pid;
    char					prio;
   	int						id;
    struct process_node	*next;
};
struct process_node *head = NULL, *tail = NULL;

/* Print a list of all tasks currently being scheduled.  */
static void
sched_print_tasks(void)
{
	struct process_node *current = head;
	printf("Printing processes...\n");
	printf("--------------------------\n");
	while (current != NULL)
	{
		if (current == head)
		{
//			printf("%c[1m", 27);
			printf("\033[1m\033[32m");
			printf("Process ID = %d, PID = %d (current)\n", current -> id, current -> pid);
			printf("\033[0m");
//			printf("%c[0m", 27);
		}
		else printf("Process ID = %d, PID = %d\n", current -> id, current -> pid);
		current = current -> next;
	}
	printf("--------------------------\n");
	printf("Printing processes... Done!\n");
}

/* Send SIGKILL to a task determined by the value of its
 * scheduler-specific id.
 */
static int
sched_kill_task_by_id(int id)
{
	struct process_node *current = head;
	while (current != NULL)
	{
		if (current -> id == id)
		{
			printf("------------------------------------------\n");
			printf("Killing process with ID = %d, PID = %d...\n", current -> id, current -> pid);
			printf("------------------------------------------\n");
			kill(current -> pid, SIGKILL);
			break;
		}
		current = current -> next;
	}
	if (current == NULL)
	{
		printf("---------------------------\n");
		printf("Requested ID was not found.\n");
		printf("---------------------------\n");
	}
	return id;
}


/* Create a new task.  */
static void
sched_create_task(char *executable)
{
	struct process_node *n, *current = head;
	char *newargv[] = { executable, NULL, NULL, NULL };
	char *newenviron[] = { NULL };
	int p, maxid = 0;
	printf("----------------------------------------\n");
	printf("Creating process...\n");
	p = fork();
	if (p < 0)
	{				/*Error*/
		perror("fork_procs: fork");
		exit(1);
	}
	if (p == 0)
	{
		printf("%ld: Stopping...\n", (long)getpid());
		raise(SIGSTOP);
		execve(executable, newargv, newenviron);
		/* execve() only returns on error */
		perror("execve");
		exit(1);
	}
	n = (struct process_node *) malloc(sizeof(struct process_node));
	n -> pid = p;
	while (current != NULL)
	{
		if ((current -> id) > maxid) maxid = current -> id;
		current = current -> next;
	} 
	n -> id  = maxid + 1;
	tail -> next = n;
	tail = n;
	printf("Created process with ID = %d, PID = %d.\n", n -> id, p);
	printf("----------------------------------------\n");
}

/* Process requests by the shell.  */
static int
process_request(struct request_struct *rq)
{
	switch (rq->request_no) {
		case REQ_PRINT_TASKS:
			sched_print_tasks();
			return 0;

		case REQ_KILL_TASK:
			return sched_kill_task_by_id(rq->task_arg);

		case REQ_EXEC_TASK:
			sched_create_task(rq->exec_task_arg);
			return 0;
		
		case REQ_HIGH_TASK:
			sched_high_task(rq->exec_task_arg);
			return 0;

		case REQ_LOW_TASK:
			sched_low_task(rq->exec_task_arg);
			return 0;

		default:
			return -ENOSYS;
	}
}

/* SIGALRM handler: Gets called whenever an alarm goes off.
 * The time quantum of the currently executing process has expired,
 * so send it a SIGSTOP. The SIGCHLD handler will take care of
 * activating the next in line.
 */
static void
sigalrm_handler(int signum)
{
	if (head != tail) kill(head -> pid, SIGSTOP);
	else alarm(SCHED_TQ_SEC);
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
					printf("****************Exited or signaled****************\n");
					head = head -> next;
					free(nxt);
					if (head == NULL)
					{
						printf("No tasks left. Exiting...\n");
						exit(0);
					}
				}
				else
				{
					kill(head -> pid, SIGSTOP);				//temporarily stop running process to manipulate list
					while (p != nxt -> pid)
					{
						prv = nxt;
						nxt = nxt -> next;
					}
					prv -> next = nxt -> next;
					if (prv -> next == NULL) tail = prv;
					free(nxt);
					kill(head -> pid, SIGCONT);				//continue running process
				}
			}
			if (WIFSTOPPED(status))
			{
				printf("****************Stopped****************\n");
				tail -> next = head;
				tail = head;
				head = head -> next;
				tail -> next = NULL;
			}
			alarm(SCHED_TQ_SEC);
			kill(head -> pid, SIGCONT);
		}
	} while (p > 0);
}

/* Disable delivery of SIGALRM and SIGCHLD. */
static void
signals_disable(void)
{
	sigset_t sigset;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	sigaddset(&sigset, SIGCHLD);
	if (sigprocmask(SIG_BLOCK, &sigset, NULL) < 0) {
		perror("signals_disable: sigprocmask");
		exit(1);
	}
}

/* Enable delivery of SIGALRM and SIGCHLD.  */
static void
signals_enable(void)
{
	sigset_t sigset;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	sigaddset(&sigset, SIGCHLD);
	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) < 0) {
		perror("signals_enable: sigprocmask");
		exit(1);
	}
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

static void
do_shell(char *executable, int wfd, int rfd)
{
	char arg1[10], arg2[10];
	char *newargv[] = { executable, NULL, NULL, NULL };
	char *newenviron[] = { NULL };

	sprintf(arg1, "%05d", wfd);
	sprintf(arg2, "%05d", rfd);
	newargv[1] = arg1;
	newargv[2] = arg2;

	raise(SIGSTOP);
	execve(executable, newargv, newenviron);

	/* execve() only returns on error */
	perror("scheduler: child: execve");
	exit(1);
}

/* Create a new shell task.
 *
 * The shell gets special treatment:
 * two pipes are created for communication and passed
 * as command-line arguments to the executable.
 */
static void
sched_create_shell(char *executable, int *request_fd, int *return_fd)
{
	struct process_node *n;
	int pfds_rq[2], pfds_ret[2];
	pid_t p;

	if (pipe(pfds_rq) < 0 || pipe(pfds_ret) < 0) {
		perror("pipe");
		exit(1);
	}

	p = fork();
	if (p < 0) {
		perror("scheduler: fork");
		exit(1);
	}

	if (p == 0) {
		/* Child */
		close(pfds_rq[0]);
		close(pfds_ret[1]);
		do_shell(executable, pfds_rq[1], pfds_ret[0]);
		assert(0);
	}
	n = (struct process_node *) malloc(sizeof(struct process_node));
	n -> pid = p;
	n -> id  = 0;
	n -> prio = "l";
	head = n;
	tail = n;

	/* Parent */
	close(pfds_rq[1]);
	close(pfds_ret[0]);
	*request_fd = pfds_rq[0];
	*return_fd = pfds_ret[1];
}

static void
shell_request_loop(int request_fd, int return_fd)
{
	int ret;
	struct request_struct rq;

	/*
	 * Keep receiving requests from the shell.
	 */
	for (;;) {
		if (read(request_fd, &rq, sizeof(rq)) != sizeof(rq)) {
			perror("scheduler: read from shell");
			fprintf(stderr, "Scheduler: giving up on shell request processing.\n");
			break;
		}

		signals_disable();
		ret = process_request(&rq);
		signals_enable();

		if (write(return_fd, &ret, sizeof(ret)) != sizeof(ret)) {
			perror("scheduler: write to shell");
			fprintf(stderr, "Scheduler: giving up on shell request processing.\n");
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	struct process_node *n;
	int nproc, p, i;
	char executable[10];
	char *newargv[] = { executable, NULL, NULL, NULL };
	char *newenviron[] = { NULL };

	/* Two file descriptors for communication with the shell */
	static int request_fd, return_fd;

	/* Create the shell. */
	sched_create_shell(SHELL_EXECUTABLE_NAME, &request_fd, &return_fd);

	/*
	 * For each of argv[1] to argv[argc - 1],
	 * create a new child process, add it to the process list.
	 */

	nproc = argc - 1; /* number of proccesses goes here */

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
		n -> prio = "l";
		tail -> next = n;
		tail = n;
	}
	
	
	/* Wait for all children to raise SIGSTOP before exec()ing. */
	wait_for_ready_children(nproc+1);

	/* Install SIGALRM and SIGCHLD handlers. */
	install_signal_handlers();

	if (nproc == 0) {
		fprintf(stderr, "Scheduler: No tasks. Exiting...\n");
		exit(1);
	}

	kill(head -> pid, SIGCONT);
	alarm(SCHED_TQ_SEC);
	
	shell_request_loop(request_fd, return_fd);

	/* Now that the shell is gone, just loop forever
	 * until we exit from inside a signal handler.
	 */
	while (pause())
		;

	/* Unreachable */
	fprintf(stderr, "Internal error: Reached unreachable point\n");
	return 1;
}
