#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

void handler(int sig)
{
	printf("\nReceived Signal : %s\n", strsignal(sig));
	fflush(stdout);

	// Let the default action do the real work (stop, continue or terminate).
	// The signal is blocked while its handler runs, so unblock it or raise() only leaves it pending.
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, sig);
	signal(sig, SIG_DFL);
	sigprocmask(SIG_UNBLOCK, &set, NULL);
	raise(sig);

	// Back here once the default action returns: after a stop, that means we were continued
	if (sig == SIGTSTP)
	{
		signal(SIGCONT, handler);
	}
	else if (sig == SIGCONT)
	{
		signal(SIGTSTP, handler);
	}
	signal(sig, handler);
}

int main(int argc, char **argv)
{
	printf("Starting the program (pid %d)\n", getpid());
	fflush(stdout);

	signal(SIGINT, handler);
	signal(SIGTSTP, handler);
	signal(SIGCONT, handler);

	while (1)
	{
		sleep(1);
	}

	return 0;
}
