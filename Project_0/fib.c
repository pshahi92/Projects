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
Unique Number: 53785
*/

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

//Prithvi driving

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
  //Prithvi driving
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

 //Abe driving now
static void 
doFib(int n, int doPrint)
{
  //int n is the argument 
  int status;
  int fib_total;
  pid_t pid, childPID;
  
  //base cases
  if(n == 0)
    fib_total += 0; //exit(0); //EXIT_SUCCESS);
  else if(n == 1)
    fib_total += 1; //exit(1); //EXIT_FAILURE);       //Prithvi driving now

  else if ((childPID = Fork()) == 0)
  {
    //inside child process

    //calling F(n-1)
    n -= 1;
    doFib(n, 0);

  }
  else //inside parent process
  {
    //calling F(n-2)
    if((childPID = Fork()) == 0)
    {  
      //inside second child
      n -= 2;
      doFib(n, 0);

    }
    else
    {
      //we have to use the WEXITSTATUS here            //Abe driving as well through debugging
      while ((pid = waitpid(-1, &status, 0)) > 0)
      {
        if(WIFEXITED(status))
        {
          fib_total += WEXITSTATUS(status);
          printf("%d\n", fib_total);
        }
      }
    }
  }
  if (doPrint)
    printf("fib %d: %d\n", n, fib_total);
  exit(fib_total);
}