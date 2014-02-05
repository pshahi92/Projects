#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "util.h"


/*
 * First, print out the process ID of this process.
 *
 * Then, set up the signal handler so that ^C causes
 * the program to print "Nice try.\n" and continue looping.
 *
 * Finally, loop forever, printing "Still here\n" once every
 * second.
 */

//helper function to handle single
 void handler(int sig)
 {
 	ssize_t bytes; 
 	const int STDOUT = 1;
 	if (sig == SIGUSR1)
 	{
 		bytes = write(STDOUT, "exiting\n", 8);
 		exit(0);
 	}
 	else if (sig == SIGINT)
 	{ 
 		bytes = write(STDOUT, "Nice try.\n", 10);
 		return;
 	}
 	else if(bytes != 10)
 		exit(-999);
 }

int main(int argc, char **argv)
{
	//signal handler
	Signal(SIGINT, handler);
	Signal(SIGUSR1, handler);

	//print pid
	pid_t pid = getpid();
	printf("%d\n", pid);

	int i = 1;
	struct timespec t = {1, 0};
	struct timespec r = {0,0};
	while(i) 
	{
		printf("Still here\n");
		while(nanosleep(&t, &r)) 
		{
			t.tv_sec = r.tv_sec;
			t.tv_nsec = r.tv_nsec;
			r.tv_sec = 0;
			r.tv_nsec = 0;
		}
			t.tv_sec = 1;
			t.tv_nsec = 0;
			r.tv_sec = 0;
			r.tv_nsec = 0;
	}
  return 0;
}


