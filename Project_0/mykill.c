#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


int main(int argc, char **argv)
{
	if(argc > 0) {
		printf("hello\n");
		int pid = atoi(argv[1]);
		kill(pid, SIGUSR1);
	}
  return 0;
}


