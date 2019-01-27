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

#define RESET   	"\033[0m"				/* Reset */
#define BLACK   	"\033[30m"   		   	/* Black */
#define RED     	"\033[31m"  	  	  	/* Red */
#define GREEN  	 	"\033[32m"  	    	/* Green */
#define YELLOW  	"\033[33m" 		     	/* Yellow */
#define BLUE   		"\033[34m"  			/* Blue */
#define MAGENTA 	"\033[35m"      		/* Magenta */
#define CYAN    	"\033[36m"      		/* Cyan */
#define WHITE		"\033[37m"      		/* White */
#define BOLDBLACK   "\033[1m\033[30m"		/* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      	/* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      	/* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      	/* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      	/* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      	/* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      	/* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      	/* Bold White */

struct process_node {
    pid_t					pid;
   	int						id;
   	char					prio;
    struct process_node		*next;
};
struct process_node *head = NULL, *tail = NULL, *hiend = NULL;
int fixproblem = 0;
/* Print a list of all tasks currently being scheduled.  */
static void
sched_print_tasks(void)
{
	struct process_node *current = head;
	printf("Printing processes...\n");
	printf("--------------------------------------------------\n");
	while (current != NULL)
	{
		if ((current == head) && (current -> prio == 'h'))
			printf(BOLDMAGENTA	"Process ID = %d, PID = %d, Priority = %c (current)\n" 	RESET, current -> id, current -> pid, current -> prio);
		else if ((current == head) && (current -> prio == 'l'))
			printf(BOLDCYAN		"Process ID = %d, PID = %d, Priority = %c (current)\n" 	RESET, current -> id, current -> pid, current -> prio);
		else if (current -> prio == 'h')
			printf(MAGENTA		"Process ID = %d, PID = %d, Priority = %c\n" 			RESET, current -> id, current -> pid, current -> prio);
		else if (current -> prio == 'l')
			printf(CYAN			"Process ID = %d, PID = %d, Priority = %c\n" 			RESET, current -> id, current -> pid, current -> prio);
		current = current -> next;
	}
	printf("--------------------------------------------------\n");
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
			printf(BOLDRED "Killing process with ID = %d, PID = %d...\n" RESET, current -> id, current -> pid);
			printf("------------------------------------------\n");
			kill(current -> pid, SIGKILL);
			break;
		}
		current = current -> next;
	}
	if (current == NULL)
	{
		printf("---------------------------\n");
		printf(BOLDRED "Requested ID was not found.\n" RESET);
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
	printf(BOLDGREEN "Creating process...\n" RESET);
	p = fork();
	if (p < 0)
	{				/*Error*/
		perror("fork_procs: fork");
		exit(1);
	}
	if (p == 0)
	{
		printf("[%ld]: Stopping...\n", (long)getpid());
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
	n -> id = maxid + 1;
	n -> prio = 'l';
	tail -> next = n;
	tail = n;
	tail -> next = NULL;
	printf(BOLDGREEN "Created process with ID = %d, PID = %d.\n" RESET, n -> id, p);
	printf("----------------------------------------\n");
}

static int
sched_high_task(int id)
{
	int found = 0, hishell = 0;
	struct process_node *current = NULL, *prev = NULL;
	
	if (hiend == NULL)
	{
		if (head -> id == id)
		{
			found = 1;
			head -> prio = 'h';
			hiend = head;
			current = head;
		}
		else if ((head != tail) && (head -> id != id))
		{
			found = 1;
			head -> prio = 'h';
			hishell = 1;
			hiend = head;
			prev = head;
			current = head -> next;
			while (current != NULL)
			{
				if (current -> id == id)
				{
					found = 1;
					current -> prio = 'h';
					prev -> next = current -> next;
					if (current == tail)
					{
						tail = prev;
						tail -> next = NULL;
					}
					current -> next = head -> next;
					head -> next = current;
					hiend = current;
					break;
				}
				prev = prev -> next;
				current = current -> next;
			}
		}
	}
	else if ((hiend != NULL) && (hiend -> next == tail) && (tail -> id == id))
	{
		found = 1;
		hiend = hiend -> next;
		hiend -> prio = 'h';
		current = hiend;
	}
	else if (hiend != NULL)
	{
		prev = hiend;
		current = hiend -> next;
		while (current != NULL)
		{
			if (current -> id == id)
			{
				found = 1;
				current -> prio = 'h';
				prev -> next = current -> next;
				if (current == tail) tail = prev;
				current -> next = hiend -> next;
				hiend -> next = current;
				hiend = current;
				break;
			}
			prev = prev -> next;
			current = current -> next;
		}
	}
	if (found)
	{
		printf("---------------------------------------------------------\n");
		printf(BOLDYELLOW "Changed priority to HIGH for process: ID = %d, PID = %d.\n" RESET, current -> id, current -> pid);
		if (hishell) printf(BOLDYELLOW "Shell [ID = %d, PID = %d] was automatically elevated to HIGH, to maintain control.\n" RESET, head -> id, head -> pid);
		printf("---------------------------------------------------------\n");
	}
	else
	{
		printf("-------------------------------------------\n");
		printf(BOLDRED "Requested ID was not found or already HIGH.\n" RESET);
		printf("-------------------------------------------\n");
	}
	return 0;
}

static int
sched_low_task(int id)
{
	int found = 0;
	struct process_node *current = NULL, *prev = NULL;
	
	if (hiend != NULL)
	{
		if ((head -> id == id) && (head != tail))
		{
			found = 1;
			fixproblem = 1;
			head -> prio = 'l';
			current = head;
			if (current == hiend) hiend = NULL;
			tail -> next = head;
			tail = head;
			head = head -> next;
			tail -> next = NULL;
		}
		else if ((head -> id == id) && (head == hiend) && (head == tail))
		{
			found = 1;
			head -> prio = 'l';
			hiend = NULL;
			current = head;
		}
		else if ((head -> id != id) && (head != hiend))
		{
			prev = head;
			current = head -> next;
			while (current != hiend -> next)
			{
				if (current -> id == id)
				{
					found = 1;
					current -> prio = 'l';
					prev -> next = current -> next;
					if (current == hiend) hiend = prev;
					tail -> next = current;
					tail = current;
					tail -> next = NULL;
					break;
				}
				prev = prev -> next;
				current = current -> next;
			}
		}
	}
	if (found == 1)
	{
		printf("--------------------------------------------------------\n");
		printf(BOLDYELLOW "Changed priority to LOW for process: ID = %d, PID = %d.\n" RESET, current -> id, current -> pid);
		printf("--------------------------------------------------------\n");
	}
	else
	{
		printf("-------------------------------------------\n");
		printf(BOLDRED "Requested ID was not found or already LOW.\n" RESET);
		printf("-------------------------------------------\n");
	}
	return 0;
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
			sched_high_task(rq->task_arg);
			return 0;

		case REQ_LOW_TASK:
			sched_low_task(rq->task_arg);
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
	if ((head == hiend) || ((hiend == NULL) && (head == tail)))
		alarm(SCHED_TQ_SEC);
	else if (fixproblem == 1)
		kill(tail -> pid, SIGSTOP);
	else kill(head -> pid, SIGSTOP);
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
	struct process_node *tmp = NULL, *prev = NULL, *current = NULL;
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
				if ((((hiend != NULL) && (head != hiend)) || ((hiend == NULL) && (head != tail))) && (head -> pid == p))
				{
					printf("DEATH CASE 1\n");
					tmp = head;
					head = head -> next;
					free(tmp);
					alarm(SCHED_TQ_SEC);
					kill(head -> pid, SIGCONT);
				}
				else if ((hiend != NULL) && (head == hiend) && (head -> pid == p))
				{
					printf("DEATH CASE 2\n");
					if (hiend -> next != NULL)
					{
						tmp = head;
						head = head -> next;
						free(tmp);
						hiend = NULL;
						alarm(SCHED_TQ_SEC);
						kill(head -> pid, SIGCONT);
					}
					else
					{
						free(head);
						head = NULL;
						hiend = NULL;
						tail = NULL;
						printf("No tasks left. Exiting...\n");
						exit(0);
					}
				}
				else if ((head -> next != NULL) && (head -> pid != p)) 
				{
					printf("DEATH CASE 3\n");
					prev = head;
					current = head -> next;
					while (current != NULL)
					{
						if (current -> pid == p)
						{
							tmp = current;
							prev -> next = current -> next;
							if (current == hiend) hiend = prev;
							if (current == tail) tail = prev;
							free(tmp);
							break;
						}
						prev = prev -> next;
						current = current -> next;
					}
				}
				else if ((hiend == NULL) && (head == tail) && (head -> pid == p))
				{
					printf("DEATH CASE 4\n");
					free(head);
					head = NULL;
					hiend = NULL;
					tail = NULL;
					printf("No tasks left. Exiting...\n");
					exit(0);
				}
			}
			if (WIFSTOPPED(status))
			{
				if (fixproblem == 1)
				{
									printf("STOP CASE 1\n");
					fixproblem = 0;
					alarm(SCHED_TQ_SEC);
					kill(head -> pid, SIGCONT);
				}
				else if ((hiend != NULL) && (head != hiend) && (head -> pid == p))
				{
									printf("STOP CASE 2\n");
					tmp = hiend -> next;
					hiend -> next = head;
					head = head -> next;
					hiend = hiend -> next;
					hiend -> next = tmp;
					alarm(SCHED_TQ_SEC);
					kill(head -> pid, SIGCONT);
				}
				else if ((hiend == NULL) && (head != tail) && (head -> pid == p))
				{
									printf("STOP CASE 3\n");
					tail -> next = head;
					head = head -> next;
					tail = tail -> next;
					tail -> next = NULL;
					alarm(SCHED_TQ_SEC);
					kill(head -> pid, SIGCONT);
				}
				else if ((hiend != NULL) && (head == hiend) && (head -> pid == p))
				{
									printf("STOP CASE 4\n");
					alarm(SCHED_TQ_SEC);
					kill(head -> pid, SIGCONT);
				}
				else if ((hiend == NULL) && (head == tail) && (head -> pid == p))
				{
									printf("STOP CASE 5\n");
					alarm(SCHED_TQ_SEC);
					kill(head -> pid, SIGCONT);
				}
			}
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
	n -> prio = 'l';
	head = n;
	tail = n;
	tail -> next = NULL;

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
			printf("[%ld]: Stopping...\n", (long)getpid());
			strncpy(executable, argv[i], sizeof(argv[i])+1);
			raise(SIGSTOP);
			execve(executable, newargv, newenviron);
			/* execve() only returns on error */
			perror("execve");
			exit(1);
		}
		n = (struct process_node *) malloc(sizeof(struct process_node));
		n -> pid = p;
		n -> id  = i;
		n -> prio = 'l';
		tail -> next = n;
		tail = n;
		tail -> next = NULL;
	}
	
	
	/* Wait for all children to raise SIGSTOP before exec()ing. */
	wait_for_ready_children(nproc+1);

	/* Install SIGALRM and SIGCHLD handlers. */
	install_signal_handlers();

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
