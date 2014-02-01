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
 	bytes = write(STDOUT, "exiting\n", 10); 
 	if(bytes != 10) 
 		exit(-999);
 	exit(1);
 }

int main(int argc, char **argv)
{
	//signal handler
	Signal(SIGUSR1, handler);

	//print pid
	pid_t pid = getpid();
	printf("%d\n", pid);

	//print "Still here\n"
	int i = 1;
	struct timespec t = {3, 0};
	while(i) 
	{
		printf("Still here\n");
		nanosleep(&t, NULL);
	}
  return 0;
}


