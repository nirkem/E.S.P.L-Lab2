#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/limits.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "LineParser.h"

#define MAX_INPUT_SIZE 2048

// Send a signal to the process named by arguments[1] ("alarm" and "blast")
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

    if (kill(pid, sig) == 0)
    {
        printf("Process %d has been %s (%s)\n", pid, verb, strsignal(sig));
    }
    else
    {
        perror(pCmdLine->arguments[0]);
    }
}

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

// Run a parsed line: built-in commands run in the shell, everything else in a child
void execute(cmdLine *pCmdLine, int debug)
{
    const char *cmd = pCmdLine->arguments[0];

    if (strcmp(cmd, "cd") == 0)
    {
        const char *dir = pCmdLine->argCount > 1 ? pCmdLine->arguments[1] : getenv("HOME");
        if (dir == NULL || chdir(dir) == -1)
        {
            perror("cd");
        }
        return;
    }

    if (strcmp(cmd, "alarm") == 0)
    {
        signalCommand(pCmdLine, SIGCONT, "woken up");
        return;
    }

    if (strcmp(cmd, "blast") == 0)
    {
        signalCommand(pCmdLine, SIGKILL, "terminated");
        return;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return;
    }

    if (pid == 0)
    { // Child process
        redirect(pCmdLine);
        execvp(cmd, pCmdLine->arguments);
        perror(cmd);
        _exit(EXIT_FAILURE); // skip the parent's exit handlers and stdio buffers
    }

    // Parent process
    if (debug)
    {
        fprintf(stderr, "PID: %d\n", pid);
        fprintf(stderr, "Executing command: %s\n", cmd);
    }

    if (pCmdLine->blocking)
    {
        waitpid(pid, NULL, 0);
    }
}

// Collect finished background children so they do not linger as zombies
static void reapBackground(void)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

int main(int argc, char *argv[])
{
    char cwd[PATH_MAX];
    char input[MAX_INPUT_SIZE];
    cmdLine *parsedLine;
    int debug = 0;

    // Check for -d flag
    if (argc > 1 && strcmp(argv[1], "-d") == 0)
    {
        debug = 1;
    }

    while (1)
    {
        reapBackground();

        // Display the prompt with the current working directory
        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
            perror("getcwd");
            exit(EXIT_FAILURE);
        }
        printf("%s> ", cwd);
        fflush(stdout);

        // Read a line of input; end of input (Ctrl-D) quits like "quit"
        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            printf("\n");
            break;
        }

        parsedLine = parseCmdLines(input);
        if (parsedLine == NULL)
        {
            continue;
        }

        if (strcmp(parsedLine->arguments[0], "quit") == 0)
        {
            freeCmdLines(parsedLine);
            break;
        }

        execute(parsedLine, debug);
        freeCmdLines(parsedLine);
    }

    return 0;
}
