#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


/*
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
Unique Number: 53785 */

/*Get the argument from command line
and send signal using kill. */
//Abe driving
int main(int argc, char **argv)
{
	if(argc > 0) {
		int pid = atoi(argv[1]);
		kill(pid, SIGUSR1);
		printf("exiting\n");
	}
  return 0;
}



