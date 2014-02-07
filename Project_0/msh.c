/* 
 * msh - A mini shell program with job control
 * 
 * <Put your name and login ID here>
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
#include "util.h"
#include "jobs.h"


/* Global variables */
int verbose = 0;            /* if true, print additional output */

extern char **environ;      /* defined in libc */
static char prompt[] = "msh> ";    /* command line prompt (DO NOT CHANGE) */
static struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
void usage(void);
void sigquit_handler(int sig);

/* ************************* */
/* routines that we've added */
void Kill(pid_t group_pid, int sig)
{
    if((kill(group_pid, sig)) < 0)
        unix_error("Kill error");
}
/* 
 * Section 8.3 of B+O
 * error handling wrapper
 */
 pid_t Fork(void)
{
    pid_t pid;
    if ((pid = fork()) < 0)
        unix_error("Fork error");
    return pid;
}

/* error handling wrapper for setp*/
void Setpgid(pid_t pid, pid_t pgid)
{
    if((setpgid(pid, pgid)) < 0)
        unix_error("Setpgid error");
}

void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    if(sigprocmask(how, set, oldset) < 0)
        unix_error("Sigprocmask error");
}

/* Print a Job, code taken from listjobs in jobs.c */
void printJob(pid_t pid);

/* ************************* */
/* ************************* */


/*
 * main - The shell's main routine 
 */
 int main(int argc, char **argv) 
 {
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) 
    {
        switch (c) 
        {
            case 'h':             /* print help message */
                usage();
                break;
            case 'v':             /* emit additional diagnostic info */
                verbose = 1;
                break;
            case 'p':             /* don't print a prompt */
                emit_prompt = 0;  /* handy for automatic testing */
                break;
            default:
                usage();
        }
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) 
    {
    	/* Read command line */
    	if (emit_prompt) 
        {
           printf("%s", prompt);
           fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
           app_error("fgets error");
    	if (feof(stdin)) 
        {   /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}

/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE]; /* Holds modified command line */
    int bg; /* Should the job run in bg or fg? */
    pid_t pid; /* Process id */

    /* what we've added*/
    sigset_t mask; /* for Sigprocmask*/
    /* *************** */

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);

    if (argv[0] == NULL)
        return;  /* Ignore empty lines */
    if (!builtin_cmd(argv)) 
    {
        /* Blocking signals to eliminate race condition */
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        Sigprocmask(SIG_BLOCK, &mask, NULL); /* Block SIG_CHLD */
        pid = Fork();
        if ( pid == 0) 
        {
            /* Child Process */
            Sigprocmask(SIG_UNBLOCK, &mask, NULL); /* Unblock SIG_CHLD */
            /* putting child in new process group, pgid == child pid */
            Setpgid(0,0);
            /* Child runs user job */
            if (execve(argv[0], argv, environ) < 0) 
            {
                printf("%s: Command not found\n", argv[0]);
                exit(0);
            }
        }
        if(bg) /* Checking BG or FG */
        {    
            addjob(jobs, pid, BG, cmdline); /* adding the child to job array as BG */
            Sigprocmask(SIG_UNBLOCK, &mask, NULL); /* Unblock SIG_CHLD */
            printJob(pid); /* printing bg job to msh cmdline */
        }
        else
        {
            addjob(jobs, pid, FG, cmdline); /* adding the child to job array as FG */
            Sigprocmask(SIG_UNBLOCK, &mask, NULL); /* Unblock SIG_CHLD */
            waitfg(pid);
            //printf("%s\n", "eval");
        }
        //printf("%s\n", "after eval");
    }
    return;
}


/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 * Return 1 if a builtin command was executed; return 0
 * if the argument passed in is *not* a builtin command.
 */
 int builtin_cmd(char **argv) 
 {
    if (!strcmp(argv[0], "quit")) /* quit command */
        exit(0);
    else if(!strcmp(argv[0], "jobs")) /* jobs command - lists all bg jobs*/
    {
        listjobs(jobs);
        return 1;
    }
    else if((!strcmp(argv[0], "fg")) || (!strcmp(argv[0], "bg"))) /* fg/bg command */
    {
        do_bgfg(argv);
        return 1;
    }
    else if (!strcmp(argv[0], "&")) /* Ignore singleton & */
        return 1;
    return 0;
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    if (!strcmp(argv[0], "bg"))
    {
        if(argv[1] != NULL)
        {
            {
                int id = atoi(argv[1]+1);
                if(id != 0)
                {
                    struct job_t* bgJob = getjobjid(jobs, id);
                    if(bgJob != NULL)
                    {
                        bgJob->state = BG;
                        Kill(-(bgJob->pid), SIGCONT);
                        printJob(bgJob->pid);
                    }
                    else
                        printf("%s%d%s\n", "%", id, ": No such job");
                }
                else
                    printf("%s\n", "bg: argument must be a PID or %jobid");
            }
        }
        else
            printf("%s\n", "bg command requires PID or %jobid argument" ); 
    }
    else if(!strcmp(argv[0], "fg"))
    {
        if(argv[1] != NULL)
        {
            {
                int id;                    
                id = atoi(argv[1]+1);

                if(id != 0) 
                {
                    struct job_t* fgJob = getjobjid(jobs, id);
                    if(fgJob != NULL)
                    {
                        if (fgJob->state == ST)
                            Kill(-(fgJob->pid), SIGCONT);
                        fgJob->state = FG;
                        waitfg(fgJob->pid);
                    }
                    else
                        printf("%s%d%s\n", "%", id, ": No such job");
                }
                else
                {
                    printf("%s\n", "fg: argument must be a PID or %jobid");
                }
            }
        }
        else
            printf("%s\n", "fg command requires PID or %jobid argument" );
    }
}



/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    while(1)
    {
        if(sleep(1))
            if(fgpid(jobs) != pid)
            {
                //printf("%s %d\n", "waitfg", pid );
                //printf("%s %d\n", "fg", fgpid(jobs) );
                fflush(stdout);
                return;
            }
    }
    
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0)
    {
        pid_t fg_job = fgpid(jobs);
        struct job_t* stpJob = getjobpid(jobs, fg_job);
        stpJob->state = ST;
            
        ssize_t bytes; 
        const int STDOUT = 1;
        
        if(WIFSTOPPED(status))
        {

            bytes = write(STDOUT, "Job [", 6);
            if(bytes != 6)
                exit(-999);

            printf("%d", stpJob->jid);
            fflush(stdout);

            bytes = write(STDOUT, "] (", 3);
            if(bytes != 3)
                exit(-999);

            printf("%d", stpJob->pid);
            fflush(stdout);

            bytes = write(STDOUT, ") stopped by signal ", 20);
            if(bytes != 20)
                exit(-999);

            printf("%d", WSTOPSIG(status));
            fflush(stdout);

            bytes = write(STDOUT, "\n", 1);
            if(bytes != 1)
                exit(-999);
            return;
        }
        else if (WIFSIGNALED(status))
        {
            bytes = write(STDOUT, "Job [", 6);
            if(bytes != 6)
                exit(-999);

            printf("%d", stpJob->jid);
            fflush(stdout);

            bytes = write(STDOUT, "] (", 3);
            if(bytes != 3)
                exit(-999);

            printf("%d", stpJob->pid);
            fflush(stdout);

            bytes = write(STDOUT, ") terminated by signal ", 23);
            if(bytes != 23)
                exit(-999);

            printf("%d", WTERMSIG(status));
            fflush(stdout);

            bytes = write(STDOUT, "\n", 1);
            if(bytes != 1)
                exit(-999);

            deletejob(jobs, pid);
            return;
        }
        else //WIFEXITED 
        {
            //printf("%s %d\n", "WIFEXITED", pid );
            deletejob(jobs, pid);
            return;
        }
    }
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    //need to get pid of fg job
    pid_t fg_job = fgpid(jobs);
    if(fg_job)
    {
        Kill(-fg_job, SIGINT);
    }
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    pid_t fg_job = fgpid(jobs);

    if(fg_job)
    {
        Kill(-fg_job, SIGTSTP);
    }
    return;
}

/*********************
 * End signal handlers
 *********************/



/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    ssize_t bytes; 
    const int STDOUT = 1;
    bytes = write(STDOUT, "Terminating after receipt of SIGQUIT signal\n", 44);
    if(bytes != 44) 
        exit(-999);
    exit(0);
}

void printJob(pid_t pid)
{
    struct job_t* jobToPrint = getjobpid(jobs, pid);
    
    printf("[%d] (%d) ", jobToPrint->jid, jobToPrint->pid);
    switch (jobToPrint->state) 
    {
        case BG: 
            //printf("Running ");
            break;
        case FG: 
            //printf("Foreground ");
            break;
        case ST: 
            printf("Stopped ");
            break;
        default:
            printf("listjobs: Internal error: job[%d].state=%d ", 
               0, jobToPrint->state);
    }
    printf("%s", jobToPrint->cmdline);
}