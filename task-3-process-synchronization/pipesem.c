#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include "pipesem.h"

void pipesem_init(struct pipesem *sem, int val)
{
	int pfd[2], i, a = 1;
	if(pipe(pfd) < 0)
	{
		perror("pipesem_init: pipe error");
		exit(1);
	}
	sem->rfd = pfd[0];
	sem->wfd = pfd[1];
	for (i = 0; i < val; i++)
	{
		if (write(sem->wfd, &a, sizeof(int)) != sizeof(int))
		{
			perror("pipesem_init: write to pipe error");
			exit(1);
		}
	}
}

void pipesem_wait(struct pipesem *sem)
{
	int a;
	if (read(sem->rfd, &a, sizeof(int)) != sizeof(int))
	{
		perror("pipesem_wait: read from pipe error");
		exit(1);
	}
}

void pipesem_signal(struct pipesem *sem)
{
	int a = 1;
	if (write(sem->wfd, &a, sizeof(int)) != sizeof(int))
	{
		perror("pipesem_signal: write to pipe error");
		exit(1);
	}
}

void pipesem_destroy(struct pipesem *sem)
{
	if (close(sem->rfd) < 0) {
		perror("pipesem_destroy: read fd close error");
		exit(1);
	}
	if (close(sem->wfd) < 0) {
		perror("pipesem_destroy: write fd close error");
		exit(1);
	}
}
