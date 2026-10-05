#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "jobs.h"

static const char *statusName(int status)
{
    switch (status)
    {
    case RUNNING:
        return "Running";
    case SUSPENDED:
        return "Suspended";
    default:
        return "Terminated";
    }
}

// Translate a waitpid() report into one of our statuses
static int statusFromWait(int wstatus)
{
    if (WIFSTOPPED(wstatus))
        return SUSPENDED;
    if (WIFCONTINUED(wstatus))
        return RUNNING;
    return TERMINATED; // exited normally or killed by a signal
}

static void freeProcess(process *p)
{
    freeCmdLines(p->cmd);
    free(p);
}

void addProcess(process **process_list, cmdLine *cmd, pid_t pid)
{
    process *p = malloc(sizeof(process));
    p->cmd = cmd;
    p->pid = pid;
    p->status = RUNNING;
    p->next = NULL;

    // Append so the list reads in launch order
    while (*process_list)
        process_list = &(*process_list)->next;
    *process_list = p;
}

void updateProcessStatus(process *process_list, int pid, int status)
{
    for (process *p = process_list; p; p = p->next)
    {
        if (p->pid == pid)
        {
            p->status = status;
            return;
        }
    }
}

void updateProcessList(process **process_list)
{
    for (process *p = *process_list; p; p = p->next)
    {
        if (p->status == TERMINATED)
            continue;

        // A process may have several pending reports (stopped, then continued...): drain them
        int wstatus;
        pid_t r;
        while ((r = waitpid(p->pid, &wstatus, WNOHANG | WUNTRACED | WCONTINUED)) == p->pid)
        {
            p->status = statusFromWait(wstatus);
            if (p->status == TERMINATED)
                break;
        }
        if (r == -1)
            p->status = TERMINATED; // already reaped, nothing left to wait for
    }
}

int waitForProcess(process *process_list, pid_t pid)
{
    int wstatus;
    int status = TERMINATED;

    if (waitpid(pid, &wstatus, WUNTRACED) == pid)
        status = statusFromWait(wstatus);
    updateProcessStatus(process_list, pid, status);
    return status;
}

void printProcessList(process **process_list)
{
    updateProcessList(process_list);

    printf("%-4s %-8s %-11s %s\n", "#", "PID", "STATUS", "COMMAND");
    int index = 0;
    for (process *p = *process_list; p; p = p->next)
    {
        printf("%-4d %-8d %-11s", index++, p->pid, statusName(p->status));
        for (int i = 0; i < p->cmd->argCount; i++)
            printf(" %s", p->cmd->arguments[i]);
        printf("\n");
    }

    // A terminated process is shown once, then forgotten
    process **link = process_list;
    while (*link)
    {
        process *p = *link;
        if (p->status == TERMINATED)
        {
            *link = p->next;
            freeProcess(p);
        }
        else
        {
            link = &p->next;
        }
    }
}

void freeProcessList(process *process_list)
{
    while (process_list)
    {
        process *next = process_list->next;
        freeProcess(process_list);
        process_list = next;
    }
}
