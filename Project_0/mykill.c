#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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




