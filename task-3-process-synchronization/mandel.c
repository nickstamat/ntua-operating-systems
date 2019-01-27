/*
 * mandel.c
 *
 * A program to draw the Mandelbrot Set on a 256-color xterm.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "mandel-lib.h"
#include "proc-common.h"
#include "pipesem.h"

#define MANDEL_MAX_ITERATION 100000

#define NCHILDREN 5



/***************************
 * Compile-time parameters *
 ***************************/

/*
 * Output at the terminal is is x_chars wide by y_chars long
*/
int y_chars = 50;
int x_chars = 130;

/*
 * The part of the complex plane to be drawn:
 * upper left corner is (xmin, ymax), lower right corner is (xmax, ymin)
*/
double xmin = -1.8, xmax = 1.0;
double ymin = -1.0, ymax = 1.0;
	
/*
 * Every character in the final output is
 * xstep x ystep units wide on the complex plane.
 */
double xstep;
double ystep;

/*
 * This function computes a line of output
 * as an array of x_char color values.
 */
void compute_mandel_line(int line, int color_val[])
{
	/*
	 * x and y traverse the complex plane.
	 */
	double x, y;

	int n;
	int val;

	/* Find out the y value corresponding to this line */
	y = ymax - ystep * line;

	/* and iterate for all points on this line */
	for (x = xmin, n = 0; x <= xmax; x+= xstep, n++) {

		/* Compute the point's color value */
		val = mandel_iterations_at_point(x, y, MANDEL_MAX_ITERATION);
		if (val > 255)
			val = 255;

		/* And store it in the color_val[] array */
		val = xterm_color(val);
		color_val[n] = val;
	}
}

/*
 * This function outputs an array of x_char color values
 * to a 256-color xterm.
 */
void output_mandel_line(int fd, int color_val[])
{
	int i;
	
	char point ='@';
	char newline='\n';

	for (i = 0; i < x_chars; i++) {
		/* Set the current color, then output the point */
		set_xterm_color(fd, color_val[i]);
		if (write(fd, &point, 1) != 1) {
			perror("compute_and_output_mandel_line: write point");
			exit(1);
		}
	}

	/* Now that the line is done, output a newline character */
	if (write(fd, &newline, 1) != 1) {
		perror("compute_and_output_mandel_line: write newline");
		exit(1);
	}
}

void compute_and_output_mandel_line(int fd, int line, struct pipesem *sem)
{
	/*
	 * A temporary array, used to hold color values for the line being drawn
	 */
	int color_val[x_chars];

	compute_mandel_line(line, color_val);
	pipesem_wait(&sem[(line%NCHILDREN)]);
	output_mandel_line(fd, color_val);
	pipesem_signal(&sem[(line+1)%NCHILDREN]);
}

void sigint_handler(int sig)
{
	signal(SIGINT, SIG_IGN);
	reset_xterm_color(1);
	signal(SIGINT, SIG_DFL);
	killpg(0, SIGINT);
}

int main(void)
{
	signal(SIGINT, sigint_handler);
	int line, i, status;
	struct pipesem sem[NCHILDREN+1];
	pid_t p;
	xstep = (xmax - xmin) / x_chars;
	ystep = (ymax - ymin) / y_chars;

	for (i = 0; i <= NCHILDREN; i++)
	{
		pipesem_init(&sem[i], 0);
	}
	for (i = 0; i < NCHILDREN; i++)
	{
		p = fork();
		if (p < 0)
		{				/*Error*/
			perror("fork_procs: fork");
			exit(1);
		}
		if (p == 0)
		{				/* Child */
			for (line = i; line < y_chars; line+=NCHILDREN)
			{
				compute_and_output_mandel_line(1, line, sem);
			}
			pipesem_signal(&sem[NCHILDREN]);
			exit(0);
		}
		else printf("Parent, PID = %ld: Created child with PID = %ld.\n", (long)getpid(), (long)p);
	}
	pipesem_signal(&sem[0]);

	for (i = 0; i < NCHILDREN; i++)
	{
		pipesem_wait(&sem[NCHILDREN]);
		pipesem_destroy(&sem[i]);
	}

	pipesem_destroy(&sem[NCHILDREN]);

	reset_xterm_color(1);

	for (i = 0; i < NCHILDREN; i++)
	{
		p = wait(&status);
		explain_wait_status(p, status);
	}

	return 0;
}
