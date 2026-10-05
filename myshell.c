#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/limits.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "LineParser.h"
#include "jobs.h"
#include "history.h"

#define MAX_INPUT_SIZE 2048

static process *processList = NULL;
static history hist;
static int debug = 0;

/* ---------- built-in commands ---------- */

// Send a signal to the process named by arguments[1] ("alarm", "suspend" and "blast")
static void signalCommand(cmdLine *pCmdLine, int sig, const char *verb)
{
    if (pCmdLine->argCount < 2)
    {
        fprintf(stderr, "usage: %s <process id>\n", pCmdLine->arguments[0]);
        return;
    }

    pid_t pid = atoi(pCmdLine->arguments[1]);
    if (pid <= 0)
    {
        fprintf(stderr, "%s: invalid process id '%s'\n", pCmdLine->arguments[0], pCmdLine->arguments[1]);
        return;
    }

    if (kill(pid, sig) == -1)
    {
        perror(pCmdLine->arguments[0]);
        return;
    }
    printf("Process %d has been %s (%s)\n", pid, verb, strsignal(sig));

    // A stop or continue is certain; whether SIGINT kills is up to the process, so waitpid decides
    if (sig == SIGTSTP)
        updateProcessStatus(processList, pid, SUSPENDED);
    else if (sig == SIGCONT)
        updateProcessStatus(processList, pid, RUNNING);
}

// Run pCmdLine if it is a built-in. Returns 1 if it was one.
static int runBuiltin(cmdLine *pCmdLine)
{
    const char *cmd = pCmdLine->arguments[0];

    if (strcmp(cmd, "cd") == 0)
    {
        const char *dir = pCmdLine->argCount > 1 ? pCmdLine->arguments[1] : getenv("HOME");
        if (dir == NULL || chdir(dir) == -1)
            perror("cd");
    }
    else if (strcmp(cmd, "procs") == 0)
        printProcessList(&processList);
    else if (strcmp(cmd, "history") == 0)
        historyPrint(&hist);
    else if (strcmp(cmd, "alarm") == 0)
        signalCommand(pCmdLine, SIGCONT, "woken up");
    else if (strcmp(cmd, "suspend") == 0)
        signalCommand(pCmdLine, SIGTSTP, "suspended");
    else if (strcmp(cmd, "blast") == 0)
        signalCommand(pCmdLine, SIGINT, "interrupted");
    else
        return 0;

    return 1;
}

/* ---------- running programs ---------- */

// Replace stdin/stdout with the redirection files. Runs in the child only.
static void redirect(cmdLine *pCmdLine)
{
    if (pCmdLine->inputRedirect != NULL)
    {
        int fd = open(pCmdLine->inputRedirect, O_RDONLY);
        if (fd == -1 || dup2(fd, STDIN_FILENO) == -1)
        {
            perror(pCmdLine->inputRedirect);
            _exit(EXIT_FAILURE);
        }
        close(fd);
    }

    if (pCmdLine->outputRedirect != NULL)
    {
        int fd = open(pCmdLine->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1 || dup2(fd, STDOUT_FILENO) == -1)
        {
            perror(pCmdLine->outputRedirect);
            _exit(EXIT_FAILURE);
        }
        close(fd);
    }
}

// Only the first command may read a file and only the last may write one; the pipe owns the rest
static int validPipeline(cmdLine *head)
{
    for (cmdLine *c = head; c; c = c->next)
    {
        if (c != head && c->inputRedirect)
        {
            fprintf(stderr, "%s: input redirection is only allowed on the first command of a pipeline\n", c->arguments[0]);
            return 0;
        }
        if (c->next && c->outputRedirect)
        {
            fprintf(stderr, "%s: output redirection is only allowed on the last command of a pipeline\n", c->arguments[0]);
            return 0;
        }
    }
    return 1;
}

static int pipelineLength(cmdLine *head)
{
    int n = 0;
    for (; head; head = head->next)
        n++;
    return n;
}

// Fork one child per command, joining neighbours with pipes. Takes ownership of head.
static void launch(cmdLine *head)
{
    if (!validPipeline(head))
    {
        freeCmdLines(head);
        return;
    }

    // Anything the shell printed must reach the output before the children's own writes do
    fflush(stdout);

    int length = pipelineLength(head);
    pid_t *pids = malloc(length * sizeof(pid_t));
    int launched = 0;

    // blocking is only set on the last command, but "&" applies to the whole pipeline
    cmdLine *last = head;
    while (last->next)
        last = last->next;
    int foreground = last->blocking;

    pid_t pgid = 0;  // background pipelines get their own process group, led by the first child
    int prevRead = -1; // read end of the pipe coming from the previous command

    cmdLine *c = head;
    while (c)
    {
        cmdLine *next = c->next;
        c->next = NULL; // each process in the list owns just its own command

        int fds[2] = {-1, -1};
        if (next && pipe(fds) == -1)
        {
            perror("pipe");
            freeCmdLines(c);
            freeCmdLines(next);
            break;
        }

        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork");
            if (next)
            {
                close(fds[0]);
                close(fds[1]);
            }
            freeCmdLines(c);
            freeCmdLines(next);
            break;
        }

        if (pid == 0)
        { // Child process
            // The shell ignores Ctrl-C and Ctrl-Z, but ignored signals survive exec: restore them
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            // Out of the terminal's process group, Ctrl-C at the prompt cannot reach a background job
            if (!foreground)
                setpgid(0, pgid);

            if (prevRead != -1)
            {
                dup2(prevRead, STDIN_FILENO);
                close(prevRead);
            }
            if (next)
            {
                close(fds[0]);
                dup2(fds[1], STDOUT_FILENO);
                close(fds[1]);
            }
            redirect(c);

            execvp(c->arguments[0], c->arguments);
            perror(c->arguments[0]);
            _exit(EXIT_FAILURE); // skip the parent's exit handlers and stdio buffers
        }

        // Parent process. Set the group here too, so it is in place whichever side runs first.
        if (!foreground)
        {
            setpgid(pid, pgid);
            if (pgid == 0)
                pgid = pid;
        }
        if (debug)
        {
            fprintf(stderr, "PID: %d\n", pid);
            fprintf(stderr, "Executing command: %s\n", c->arguments[0]);
        }

        addProcess(&processList, c, pid);
        pids[launched++] = pid;

        // Every copy of a write end must be closed, or the reader never sees EOF
        if (prevRead != -1)
            close(prevRead);
        if (next)
            close(fds[1]);
        prevRead = fds[0];
        c = next;
    }

    if (prevRead != -1)
        close(prevRead); // only left open when a later fork failed

    if (foreground)
    {
        for (int i = 0; i < launched; i++)
        {
            if (waitForProcess(processList, pids[i]) == SUSPENDED)
                printf("\nProcess %d suspended (resume it with: alarm %d)\n", pids[i], pids[i]);
        }
    }
    free(pids);
}

/* ---------- the prompt loop ---------- */

static int isNumber(const char *s)
{
    if (*s == 0)
        return 0;
    for (; *s; s++)
        if (!isdigit((unsigned char)*s))
            return 0;
    return 1;
}

static int isBlank(const char *s)
{
    for (; *s; s++)
        if (!isspace((unsigned char)*s))
            return 0;
    return 1;
}

// True for lines like "> out.txt" or "ls | > x", where a stage has no program to run
static int hasEmptyCommand(cmdLine *head)
{
    for (; head; head = head->next)
        if (head->argCount == 0)
            return 1;
    return 0;
}

// Replace "!!" or "!n" in line with the command it refers to. Returns 0 if there is no such entry.
static int expandHistory(char *line, size_t size)
{
    if (line[0] != '!')
        return 1;

    const char *previous = NULL;
    if (strcmp(line, "!!") == 0)
        previous = historyLast(&hist);
    else if (isNumber(line + 1))
        previous = historyGet(&hist, atoi(line + 1));

    if (previous == NULL)
    {
        printf("%s: event not found\n", line);
        return 0;
    }

    // Copy first: adding the expanded line to the history may overwrite this very slot
    snprintf(line, size, "%s", previous);
    printf("%s\n", line);
    return 1;
}

int main(int argc, char *argv[])
{
    char cwd[PATH_MAX];
    char input[MAX_INPUT_SIZE];

    if (argc > 1 && strcmp(argv[1], "-d") == 0)
        debug = 1;

    // Ctrl-C and Ctrl-Z are meant for the job in the foreground, not the shell
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    historyInit(&hist);

    while (1)
    {
        // Notice background jobs that finished, so they are reaped instead of left as zombies
        updateProcessList(&processList);

        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
            perror("getcwd");
            break;
        }
        printf("%s> ", cwd);
        fflush(stdout);

        // End of input (Ctrl-D) quits like "quit"
        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            printf("\n");
            break;
        }
        input[strcspn(input, "\n")] = 0;

        if (isBlank(input) || !expandHistory(input, sizeof(input)))
            continue;
        historyAdd(&hist, input); // the unparsed line, so "!n" can re-parse it

        cmdLine *parsedLine = parseCmdLines(input);
        if (parsedLine == NULL)
            continue;
        if (hasEmptyCommand(parsedLine))
        {
            fprintf(stderr, "syntax error: missing command\n");
            freeCmdLines(parsedLine);
            continue;
        }

        if (strcmp(parsedLine->arguments[0], "quit") == 0)
        {
            freeCmdLines(parsedLine);
            break;
        }

        // Built-ins change the shell itself, so they never run in a child or a pipeline
        if (parsedLine->next == NULL && runBuiltin(parsedLine))
            freeCmdLines(parsedLine);
        else
            launch(parsedLine);
    }

    freeProcessList(processList);
    return 0;
}
