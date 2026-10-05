# myshell: a Unix shell in C

[![test](https://github.com/nirkem/E.S.P.L-Lab2/actions/workflows/test.yml/badge.svg)](https://github.com/nirkem/E.S.P.L-Lab2/actions/workflows/test.yml)

A command interpreter built directly on the Linux process API: `fork`, `execvp`, `waitpid`, `kill`, `pipe` and `dup2`. It runs pipelines with redirection, keeps track of every process it starts, and remembers your last 20 commands. Written for the Extended Systems Programming Lab (ESPL) at Ben-Gurion University in 2023 (labs 2 and C).

```
/tmp/demo> ./looper &
Starting the program (pid 818)
/tmp/demo> suspend 818
Process 818 has been suspended (Stopped)
Received Signal : Stopped
/tmp/demo> cat < notes.txt | sort | uniq -c | sort -rn
      3 pipes
      2 signals
      1 fork
      1 exec
/tmp/demo> procs
#    PID      STATUS      COMMAND
0    818      Suspended   ./looper
1    832      Terminated  cat
2    834      Terminated  sort
3    835      Terminated  uniq -c
4    836      Terminated  sort -rn
/tmp/demo> alarm 818
Process 818 has been woken up (Continued)
Received Signal : Continued
/tmp/demo> blast 818
Process 818 has been interrupted (Interrupt)
Received Signal : Interrupt
/tmp/demo> !3
cat < notes.txt | sort | uniq -c | sort -rn
      3 pipes
      ...
```

## Features

**Running programs**
- Anything that is not a built-in runs in a child process (`fork` + `execvp`), found through `PATH`.
- A trailing `&` runs it in the background. Otherwise the shell waits for it.
- `cmd < in.txt > out.txt` swaps the child's stdin/stdout with `dup2` before `exec`, so the shell's own streams are never touched. Output files are truncated, and a missing input file is an error rather than being silently created.

**Pipelines**
- `a | b | c`, any length. Each stage is its own child, joined to the next by a pipe.
- Redirection works at the ends: `cat < in.txt | sort | head -n 3 > top.txt`. Redirecting the middle of a pipeline (the output of a stage that feeds a pipe, or the input of one that reads from it) is rejected before any process starts.

**Process manager**
- Every process the shell starts is kept in a linked list with its status: Running, Suspended or Terminated.
- `procs` prints the list. A finished process is shown once, then dropped.
- `suspend <pid>` sends `SIGTSTP`, `alarm <pid>` sends `SIGCONT`, `blast <pid>` sends `SIGINT`.
- Statuses come from `waitpid` with `WNOHANG | WUNTRACED | WCONTINUED`, so a process that is stopped or continued from outside the shell is tracked too. Finished background jobs are collected before each prompt, so none are left behind as zombies.

**Ctrl-C and Ctrl-Z**
- The shell ignores both, so they only reach the job in the foreground.
- A foreground job stopped with Ctrl-Z gives the prompt back and shows as Suspended. `alarm <pid>` resumes it.
- Background jobs run in their own process group, so Ctrl-C at the prompt does not kill them.

**History**
- The last 20 command lines, kept unparsed in a circular queue.
- `history` lists them. `!!` re-runs the last one and `!n` re-runs entry `n`. The line it expands to is printed, stored again and parsed fresh, so it works with pipes and redirection.

**Other built-ins:** `cd <dir>` (no argument goes to `$HOME`), `quit` (or Ctrl-D). `./myshell -d` prints each child's PID and command to stderr.

## The other programs

- **`looper`** is a target for the signal commands. It catches `SIGINT`, `SIGTSTP` and `SIGCONT`, prints the signal's name, then hands it to the default action so the process really stops, continues or exits. It re-arms its handlers afterwards, so it can be stopped and woken any number of times.
- **`mypipeline`** runs `ls -l | tail -n 2` by hand, narrating every fork, `dup` and `close` on stderr.
- **`mypipe`** is the warm-up: a child sends `hello` to its parent through a pipe.

## Things worth knowing

- **Why the pipe hangs if you forget one `close`.** A reader only sees end-of-file once every copy of the write end is closed, including the copies the shell and the other children inherited from `fork`. Leave one open and `wc` waits forever.
- **Why children reset `SIGINT` before `exec`.** `exec` resets handled signals to their default but keeps ignored ones ignored. Since the shell ignores Ctrl-C, its children would too unless they restore it.
- **Why the looper unblocks the signal before `raise`.** While a handler runs, the kernel blocks that same signal, so `raise` there only leaves it pending. Once the handler re-arms itself, the pending signal fires it again, forever. `sigprocmask(SIG_UNBLOCK, ...)` lets the default action happen right away.
- **Why `_exit` after a failed `execvp`.** The child is a copy of the shell, including its unflushed stdio buffers. `exit` would flush them a second time; `_exit` leaves immediately.
- **Why `suspend` and not `sleep`.** The lab spec named the command `sleep <pid>`, which would hide the real `sleep` program. Built-ins win over `PATH`, so it got a name of its own.

## Build, run, test

Linux (or WSL) with `gcc` and `make`:

```bash
make
./myshell
make test
```

`make test` runs 50 end-to-end checks in [`tests/run.sh`](tests/run.sh). They drive the shell through a FIFO, so a test can read a PID the shell printed and signal that process. If `valgrind` is installed, the suite also checks a full session for leaks. CI runs everything on every push.

## Files

| File | Role |
| --- | --- |
| `myshell.c` | Prompt loop, built-ins, pipelines and redirection |
| `jobs.c`, `jobs.h` | The process list: add, update from `waitpid`, print, free |
| `history.c`, `history.h` | The 20-entry circular history queue |
| `LineParser.c`, `LineParser.h` | Command-line parser (arguments, `<`, `>`, `&`, `\|`). Provided by the course staff |
| `Looper.c` | Signal-handling test program |
| `mypipeline.c`, `mypipe.c` | Standalone pipe exercises |
| `tests/run.sh` | End-to-end test suite |
