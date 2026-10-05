#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>
#include "LineParser.h"

#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0

typedef struct process
{
    cmdLine *cmd;         /* the parsed command line (owned by this node) */
    pid_t pid;            /* the process id that is running the command */
    int status;           /* RUNNING, SUSPENDED or TERMINATED */
    struct process *next; /* next process in chain */
} process;

/* Appends a RUNNING process to the end of the list. The list takes ownership of cmd. */
void addProcess(process **process_list, cmdLine *cmd, pid_t pid);

/* Refreshes every status, prints the list, then drops the processes reported as terminated */
void printProcessList(process **process_list);

/* Frees every node and its command line */
void freeProcessList(process *process_list);

/* Polls each live process with waitpid(WNOHANG) and records stops, continues and exits */
void updateProcessList(process **process_list);

/* Sets the status of the process with the given pid, if it is in the list */
void updateProcessStatus(process *process_list, int pid, int status);

/* Blocks until pid exits or is stopped (Ctrl-Z), records it and returns the new status */
int waitForProcess(process *process_list, pid_t pid);

#endif
