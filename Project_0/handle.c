#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "util.h"

/*
*
################
TEAM INFO
################
Name1: Prithvi Shahi
EID1: pbs428
CS login: pshahi92
Email: shahi.prithvi@gmail.com
Unique Number: 53785

Name2: Abraham Munoz
EID2: am56438
CS login: abemunoz
Email: abrahamunoz@utexas.edu
Unique Number: 53785
/*
 * First, print out the process ID of this process.
 *
 * Then, set up the signal handler so that ^C causes
 * the program to print "Nice try.\n" and continue looping.
 *
 * Finally, loop forever, printing "Still here\n" once every
 * second.
 */
//Abe driving
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
//Prithvi driving now
int main(int argc, char **argv)
{
	//signal handlers
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
		//implementing the print still here in every one second interval
		while(nanosleep(&t, &r)) 
		{
			//this is reseting the timespec struct to the correct times
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
//Abe drove through debugging 


