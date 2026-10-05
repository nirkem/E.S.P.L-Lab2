#!/usr/bin/env bash
# End-to-end tests: feed command lines to ./myshell and check what comes back.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
SHELL_BIN="$ROOT/myshell"
WORK=$(mktemp -d)
SH=""
pass=0
fail=0

cleanup() {
    [ -n "$SH" ] && kill -9 "$SH" 2>/dev/null
    pkill -9 -f "$ROOT/looper" 2>/dev/null
    rm -rf "$WORK"
}
trap cleanup EXIT
cd "$WORK" || exit 1

ok()   { pass=$((pass + 1)); echo "  ok    $1"; }
bad()  { fail=$((fail + 1)); echo "  FAIL  $1"; shift; printf '        %s\n' "$@"; }

# check NAME EXPECTED ACTUAL: ACTUAL must contain EXPECTED
check() {
    if [[ "$3" == *"$2"* ]]; then ok "$1"; else bad "$1" "expected to contain: $2" "got: ${3:0:600}"; fi
}
check_not() {
    if [[ "$3" != *"$2"* ]]; then ok "$1"; else bad "$1" "expected NOT to contain: $2" "got: ${3:0:600}"; fi
}
# check_re NAME REGEX ACTUAL
check_re() {
    if [[ "$3" =~ $2 ]]; then ok "$1"; else bad "$1" "expected to match: $2" "got: ${3:0:600}"; fi
}
check_eq() {
    if [[ "$3" == "$2" ]]; then ok "$1"; else bad "$1" "expected: $2" "got: $3"; fi
}

# Batch mode: one argument per input line, stdout and stderr together
run() { printf '%s\n' "$@" | timeout 10 "$SHELL_BIN" 2>&1; }

# Session mode: a live shell reading from a FIFO, so a test can react to what it prints
start_session() {
    rm -f in.fifo out.log
    mkfifo in.fifo
    "$SHELL_BIN" < in.fifo > out.log 2>&1 &
    SH=$!
    exec 3> in.fifo
}
send() { echo "$1" >&3; sleep "${2:-0.3}"; }
# say LINE: send it and print only the output it produced
say() {
    local mark
    mark=$(wc -c < out.log)
    send "$1"
    tail -c +$((mark + 1)) out.log
}
end_session() {
    echo quit >&3
    exec 3>&-
    wait "$SH" 2>/dev/null
    SH=""
}

echo "running commands"
check "runs a program" "hello world" "$(run 'echo hello world')"
check "prompt shows the working directory" "$WORK> " "$(run '')"
check "unknown command is reported" "nosuchcmd: No such file or directory" "$(run nosuchcmd)"
check "cd changes the shell's directory" "/> " "$(run 'cd /' 'pwd')"
check "cd to a missing directory fails" "cd: No such file or directory" "$(run 'cd /does/not/exist')"
check "a line with no command is rejected" "syntax error: missing command" "$(run '> nowhere.txt')"
echo -n | timeout 5 "$SHELL_BIN" > /dev/null
check_eq "Ctrl-D (end of input) exits cleanly" "0" "$?"

echo "redirection"
printf 'b\na\nc\n' > in.txt
echo "a much longer line that must not survive" > out.txt
run 'sort < in.txt > out.txt' > /dev/null
check_eq "< and > together, output truncated" $'a\nb\nc' "$(cat out.txt)"
check "missing input file is an error" "nope.txt: No such file or directory" "$(run 'cat < nope.txt')"
[ ! -e nope.txt ] && ok "missing input file is not created" || bad "missing input file is not created"

echo "pipelines"
check "two-stage pipe" "3" "$(run 'echo one two three | wc -w')"
run 'cat < in.txt | sort -r | head -n 1 > top.txt' > /dev/null
check_eq "three stages with redirection at both ends" "c" "$(cat top.txt)"
check "long pipe sees EOF and finishes" "3" "$(run 'cat in.txt | cat | cat | cat | wc -l')"
out=$(run 'ls > x.txt | wc -l')
check "output redirect mid-pipeline is rejected" "only allowed on the last command" "$out"
[ ! -e x.txt ] && ok "rejected pipeline starts no process" || bad "rejected pipeline starts no process"
check "input redirect mid-pipeline is rejected" "only allowed on the first command" "$(run 'ls | wc -l < in.txt')"

echo "history"
out=$(run 'echo first' 'echo second' 'history')
check "history lists entries in order" $'  1  echo first\n' "$out"
check "history includes itself" "  3  history" "$out"
out=$(run 'echo again' '!!')
check "!! prints the command it repeats, then runs it" $'> echo again\nagain' "$out"
[ "$(grep -c '^again$' <<< "${out//> /$'\n'}")" -eq 2 ] && ok "!! runs it again" || bad "!! runs it again" "got: $out"
out=$(run 'echo a1' 'echo b2' '!1' 'history')
check "!n re-runs entry n and records it" "  3  echo a1" "$out"
check "!n prints the expansion before the output" $'> echo a1\na1' "$(run 'echo a1' '!1')"
check "!n out of range is reported" "!7: event not found" "$(run 'echo x' '!7')"
lines=()
for i in $(seq 1 25); do lines+=("echo n$i"); done
out=$(run "${lines[@]}" history)
check "history keeps the newest 20 (oldest dropped)" "  1  echo n7" "$out"
check "history wraps around the circular buffer" " 20  history" "$out"
check_not "history forgets entries past 20" "echo n6" "$(grep '^ *[0-9]' <<< "$out")"

echo "process manager"
out=$(run 'sleep 0.1' 'procs' 'procs')
check "a finished foreground job is listed once as Terminated" "Terminated  sleep 0.1" "$out"
[ "$(grep -c 'sleep 0.1' <<< "$out")" -eq 1 ] && ok "terminated jobs are dropped after being shown" || bad "terminated jobs are dropped after being shown" "got: $out"
check "background job is Running" "Running     sleep 1" "$(run 'sleep 1 &' 'procs')"

start_session
send "$ROOT/looper &" 0.5
pid=$(grep -o 'Starting the program (pid [0-9]*' out.log | grep -o '[0-9]*$')
if [ -z "$pid" ]; then
    bad "looper started in the background" "log: $(cat out.log)"
else
    ok "looper started in the background"
    check "suspend sends SIGTSTP" "has been suspended" "$(say "suspend $pid")"
    check_re "suspended looper shows Suspended" "$pid +Suspended" "$(say procs)"
    check "alarm sends SIGCONT" "has been woken up" "$(say "alarm $pid")"
    check_re "woken looper shows Running" "$pid +Running" "$(say procs)"
    say "suspend $pid" > /dev/null
    check_re "looper can be suspended a second time" "$pid +Suspended" "$(say procs)"
    say "alarm $pid" > /dev/null
    check_re "and woken a second time" "$pid +Running" "$(say procs)"
    check "blast sends SIGINT" "has been interrupted" "$(say "blast $pid")"
    check_re "blasted looper shows Terminated" "$pid +Terminated" "$(say procs)"
    check_not "and is then forgotten" "$pid" "$(say procs)"
    check_eq "looper reported each signal" "Stopped Continued Stopped Continued Interrupt" \
        "$(grep -o 'Received Signal : .*' out.log | cut -d' ' -f4 | paste -sd' ')"
fi

# A foreground job that gets stopped (what Ctrl-Z does) hands the prompt back
printf '#!/bin/sh\nkill -STOP $$\necho resumed-ok\n' > stopme.sh
chmod +x stopme.sh
out=$(say ./stopme.sh)
check "stopped foreground job returns the prompt" "suspended (resume it with: alarm" "$out"
spid=$(grep -o 'Process [0-9]* suspended' <<< "$out" | grep -o '[0-9]*')
check "stopped foreground job shows Suspended" "Suspended   ./stopme.sh" "$(say procs)"
check "alarm resumes it" "resumed-ok" "$(say "alarm $spid" 0.5)"

# Ctrl-C belongs to the foreground job, not the shell
kill -INT "$SH"
check "the shell ignores SIGINT" "still-here" "$(say 'echo still-here')"
send "sleep 30" 0.3
fg=$(pgrep -P "$SH" -x sleep)
kill -INT "$fg"
sleep 0.3
check "a foreground job still dies on SIGINT" "back-at-prompt" "$(say 'echo back-at-prompt')"
send "sleep 30 &" 0.3
bg=$(pgrep -P "$SH" -x sleep)
if [ -n "$bg" ] && [ "$(ps -o pgid= -p "$bg" | tr -d ' ')" != "$(ps -o pgid= -p "$SH" | tr -d ' ')" ]; then
    ok "background job runs in its own process group"
else
    bad "background job runs in its own process group"
fi
[ -n "$bg" ] && kill "$bg"
end_session

echo "pipe exercises"
check_eq "mypipe" "Parent received: hello" "$("$ROOT/mypipe")"
expected=$(ls -l | tail -n 2)
check_eq "mypipeline matches ls -l | tail -n 2" "$expected" "$("$ROOT/mypipeline" 2>/dev/null)"
check "mypipeline narrates on stderr" "(parent_process>exiting...)" "$("$ROOT/mypipeline" 2>&1 >/dev/null)"

if command -v valgrind > /dev/null; then
    echo "memory"
    printf '%s\n' 'echo hi' 'echo a b | wc -w' 'cat < in.txt | sort > s.txt' 'history' '!!' '!1' \
        'sleep 0.1' 'sleep 5 &' 'procs' 'cd /' 'blast' 'ls | wc -l < in.txt' 'quit' |
        valgrind --leak-check=full --errors-for-leak-kinds=definite,indirect --error-exitcode=99 \
            "$SHELL_BIN" > /dev/null 2> valgrind.log
    [ $? -ne 99 ] && ok "no leaks or memory errors (valgrind)" || bad "no leaks or memory errors (valgrind)" "$(tail -n 30 valgrind.log)"
fi

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
