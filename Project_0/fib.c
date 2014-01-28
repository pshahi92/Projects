#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

const int MAX = 13;

static void doFib(int n, int doPrint);

static pid_t Fork(void);

/*
 * unix_error - unix-style error routine.
 */
inline static void 
unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}



/* 
 * Section 8.3 of B+O
 * error handling wrapper
 */
static pid_t 
Fork(void)
{
  pid_t pid;
  if ((pid = fork()) < 0)
    unix_error("Fork error");
  return pid;
}




int main(int argc, char **argv)
{
  int arg;
  int print;

  if(argc != 2){
    fprintf(stderr, "Usage: fib <num>\n");
    exit(-1);
  }

  if(argc >= 3){
    print = 1;
  }

  arg = atoi(argv[1]);
  if(arg < 0 || arg > MAX){
    fprintf(stderr, "number must be between 0 and %d\n", MAX);
    exit(-1);
  }

  doFib(arg, 1);

  return 0;
}

/* 
 * Recursively compute the specified number. If print is
 * true, print it. Otherwise, provide it to my parent process.
 *
 * NOTE: The solution must be recursive and it must fork
 * a new child for each call. Each process should call
 * doFib() exactly once.
 */
static void 
doFib(int n, int doPrint)
{
  if(n == 0)
    exit(EXIT_SUCCESS);
  if(n == 1)
    exit(EXIT_FAILURE);
  //int n is the argument 
  pid_t childPID;
  switch (childPID = Fork())
  {
    case 0: //inside child process
    {
      n -= 1;
      doFib(n, 0);

      break;
    }
    default://inside parent process
    {
      //we have to use the WEXITSTATUS here    
    }
  }
}


