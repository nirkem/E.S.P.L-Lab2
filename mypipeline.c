#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

// Runs "ls -l | tail -n 2" with two children joined by a pipe, narrating each step on stderr
int main(void)
{
    char *const lsArgs[] = {"ls", "-l", NULL};
    char *const tailArgs[] = {"tail", "-n", "2", NULL};
    int fds[2];

    if (pipe(fds) == -1)
    {
        perror("pipe");
        return EXIT_FAILURE;
    }

    fprintf(stderr, "(parent_process>forking...)\n");
    pid_t child1 = fork();
    if (child1 == -1)
    {
        perror("fork");
        return EXIT_FAILURE;
    }
    if (child1 == 0)
    {
        fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe...)\n");
        close(STDOUT_FILENO);
        dup(fds[1]); // lands on fd 1, the lowest free descriptor
        close(fds[1]);
        close(fds[0]);
        fprintf(stderr, "(child1>going to execute cmd: ls -l)\n");
        execvp(lsArgs[0], lsArgs);
        perror("execvp");
        _exit(EXIT_FAILURE);
    }
    fprintf(stderr, "(parent_process>created process with id: %d)\n", child1);

    // Without this, tail would never see EOF: the parent would still hold a write end
    fprintf(stderr, "(parent_process>closing the write end of the pipe...)\n");
    close(fds[1]);

    fprintf(stderr, "(parent_process>forking...)\n");
    pid_t child2 = fork();
    if (child2 == -1)
    {
        perror("fork");
        return EXIT_FAILURE;
    }
    if (child2 == 0)
    {
        fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe...)\n");
        close(STDIN_FILENO);
        dup(fds[0]); // lands on fd 0
        close(fds[0]);
        fprintf(stderr, "(child2>going to execute cmd: tail -n 2)\n");
        execvp(tailArgs[0], tailArgs);
        perror("execvp");
        _exit(EXIT_FAILURE);
    }
    fprintf(stderr, "(parent_process>created process with id: %d)\n", child2);

    fprintf(stderr, "(parent_process>closing the read end of the pipe...)\n");
    close(fds[0]);

    fprintf(stderr, "(parent_process>waiting for child processes to terminate...)\n");
    waitpid(child1, NULL, 0);
    waitpid(child2, NULL, 0);

    fprintf(stderr, "(parent_process>exiting...)\n");
    return EXIT_SUCCESS;
}
