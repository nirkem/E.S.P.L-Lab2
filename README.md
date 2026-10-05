# myshell: a small Unix shell in C

A command interpreter built directly on the Linux process API: `fork`, `execvp`, `waitpid`, `kill`, `dup2` and `pipe`. Written for the Extended Systems Programming Lab (ESPL) at Ben-Gurion University.

```
/home/nir/lab2> ./looper &
Starting the program (pid 4127)
/home/nir/lab2> cat < notes.txt > copy.txt
/home/nir/lab2> cd /tmp
/tmp> blast 4127
Process 4127 has been terminated (Killed)
/tmp> quit
```

## What it does

- **Runs programs.** Every command that is not a built-in is started in a child process with `fork` + `execvp`, so `ls`, `cat` or any program on `PATH` works.
- **Foreground and background.** A trailing `&` runs the command in the background and the prompt comes back immediately. Otherwise the shell waits for it with `waitpid`. Finished background jobs are reaped before every prompt, so they never pile up as zombies.
- **I/O redirection.** `cmd < in.txt > out.txt` swaps the child's stdin/stdout with `dup2` before `exec`, so the shell's own streams are never touched. Output files are truncated, and a missing input file is an error rather than being silently created.
- **Built-ins that must run inside the shell:**
  - `cd <dir>` changes the shell's own working directory (a child could not do it for us). With no argument it goes to `$HOME`.
  - `alarm <pid>` sends `SIGCONT` to wake a stopped process.
  - `blast <pid>` sends `SIGKILL` to terminate one.
  - `quit` (or Ctrl-D) exits.
- **Debug mode.** `./myshell -d` prints each child's PID and command to stderr.

## The other programs

- **`looper`** is a test target for the signal commands. It catches `SIGINT`, `SIGTSTP` and `SIGCONT`, prints the signal's name, then hands it to the default handler so the process really stops, continues or exits. After a stop/continue cycle it re-arms its handlers, so it can be stopped and woken any number of times.
- **`mypipe`** is a warm-up for shell pipes: a child writes `hello` into a pipe and the parent reads and prints it, each side closing the end it does not use.

## Things worth knowing

- **Why `_exit` after a failed `execvp`?** The child is a copy of the shell, including its unflushed stdio buffers. `exit` would flush them a second time; `_exit` leaves immediately.
- **Why the looper unblocks the signal before `raise`?** While a handler runs, the kernel blocks that same signal. Calling `raise` there only leaves it pending, and once the handler re-arms itself it would fire again, forever. `sigprocmask(SIG_UNBLOCK, ...)` lets the default action happen right away.
- **Why wildcards like `ls *` do not expand?** Globbing is a shell feature, not a kernel one. `execvp` passes `*` to `ls` as a literal argument.

## Build and run

Linux (or WSL) with `gcc` and `make`:

```bash
make
./myshell        # or ./myshell -d for debug output
```

Try the signal commands from inside the shell:

```
./looper &
ps
alarm <pid>
blast <pid>
```

## Files

| File | Role |
| --- | --- |
| `myshell.c` | The shell: prompt loop, built-ins, fork/exec, redirection |
| `LineParser.c`, `LineParser.h` | Command-line parser (arguments, `<`, `>`, `&`, `\|`). Provided by the course staff |
| `Looper.c` | Signal-handling test program |
| `mypipe.c` | Parent/child pipe exercise |
| `makefile` | Builds `myshell`, `looper` and `mypipe` |
