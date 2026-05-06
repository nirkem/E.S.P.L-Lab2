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

// Function to execute the parsed command
void execute(cmdLine *pCmdLine, int debug)
{

    // Check if the command is "alarm"
    if (strcmp(pCmdLine->arguments[0], "alarm") == 0)
    {
        pid_t pid = atoi(pCmdLine->arguments[1]);
        if (kill(pid, SIGCONT) == 0)
        {
            printf("Process %d has been woken up (SIGCONT)\n", pid);
        }
        else
        {
            perror("alarm failed");
        }
        return;
    }

    // Check if the command is "blast"
    if (strcmp(pCmdLine->arguments[0], "blast") == 0)
    {
        pid_t pid = atoi(pCmdLine->arguments[1]);
        if (kill(pid, SIGKILL) == 0)
        {
            printf("Process %d has been terminated (SIGKILL)\n", pid);
        }
        else
        {
            perror("blast failed");
        }
        return;
    }
    pid_t pid;
    int status;

    pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    { // Child process

        // Handle input redirection
        if (pCmdLine->inputRedirect != NULL)
        {
            close(STDIN_FILENO);
            int input_fd = open(pCmdLine->inputRedirect, O_CREAT, 0777);
            if (input_fd == -1)
            {
                perror("open input file");
                _exit(EXIT_FAILURE);
            }               
        }

        // Handle output redirection
        if (pCmdLine->outputRedirect != NULL)
        {
            close(STDOUT_FILENO);
            int output_fd = open(pCmdLine->outputRedirect, O_WRONLY | O_CREAT, 0777);
            if (output_fd == -1)
            {
                perror("open output file");
                _exit(EXIT_FAILURE);
            }              
        }

        // Execute the command
        if (execvp(pCmdLine->arguments[0], pCmdLine->arguments) == -1)
        {
            perror("execvp");
            _exit(EXIT_FAILURE);
        }

    }
    else
    { // Parent process
        if (pCmdLine->blocking)
        {
            waitpid(pid, &status, 0);
        }
    }

    // Print debug information if enabled
    if (debug)
    {
        fprintf(stderr, "PID: %d\n", pid);
        fprintf(stderr, "Executing command: %s\n", pCmdLine->arguments[0]);
    }
}

int main(int argc, char *argv[]) {
    char cwd[PATH_MAX];
    char input[2048];
    cmdLine *parsedLine;
    int debug = 0;

    // Check for -d flag
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        debug = 1;
    }

    while (1) {
        // Display the prompt with the current working directory
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("getcwd failed");
            exit(EXIT_FAILURE);
        }
        printf("%s> ", cwd);
        fflush(stdout);

        // Read a line of input from the user
        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("fgets failed");
            exit(EXIT_FAILURE);
        }

        // Parse the input command line
        parsedLine = parseCmdLines(input);
        if (parsedLine == NULL) {
            continue;
        }

        // Check for "quit" command
        if (strcmp(parsedLine->arguments[0], "quit") == 0) {
            freeCmdLines(parsedLine);
            break;
        }

        // Execute the parsed command
        execute(parsedLine, debug);

        // Free the parsed command line structure
        freeCmdLines(parsedLine);
    }

    return 0;
}
