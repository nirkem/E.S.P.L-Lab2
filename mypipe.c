#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// A child sends one message to its parent through a pipe
int main(void)
{
    const char *message = "hello";
    char buffer[64];
    int fds[2];

    if (pipe(fds) == -1)
    {
        perror("pipe");
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0)
    { // Child: keeps only the write end
        close(fds[0]);
        write(fds[1], message, strlen(message));
        close(fds[1]);
        _exit(EXIT_SUCCESS);
    }

    // Parent: keeps only the read end
    close(fds[1]);
    ssize_t n = read(fds[0], buffer, sizeof(buffer) - 1);
    close(fds[0]);
    waitpid(pid, NULL, 0);

    if (n < 0)
    {
        perror("read");
        return EXIT_FAILURE;
    }
    buffer[n] = 0;
    printf("Parent received: %s\n", buffer);
    return EXIT_SUCCESS;
}
