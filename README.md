C Shell

A POSIX-compliant custom shell implemented in C.
Supports interactive prompt, input parsing, built-in commands (hop, reveal, log, etc.), pipes, I/O redirection, sequential and background execution, and process control.

COMPILATION

Make sure you are in the correct directory, compile using 
#make clean

followed by
 
#make all

Implementation Overview

SHELL INPUT

Prompt: <Username@SystemName:current_path>

Displays ~ for home-relative paths.

Reads user input line-by-line.

Parses commands according to given grammar.

Detects invalid syntax → prints Invalid Syntax!.

SHELL INTRINSICS

hop — Change directory
Supports ~, ., .., -, and named directories.
Handles invalid paths with No such directory!.

reveal — List directory contents
Supports flags -a, -l (combined forms accepted).
Lists files lexicographically, hidden files shown with -a.
Errors:

Too many arguments → reveal: Invalid Syntax!

No previous directory → No such directory!

log — Persistent command history (max 15)

Persists across sessions.

Ignores consecutive duplicates and log commands.

log — Show history

log purge — Clear history

log execute <index> — Execute command by index

REDIRECTION AND PIPES

Input Redirection:

< file — Reads STDIN from file.

Missing file → No such file or directory.

Output Redirection:

> file — Truncate and write.

>> file — Append mode.

File creation failure → Unable to create file for writing.

Piping:

Connects multiple commands via |.

Supports mixed use with redirections.

Sequential (;) — Executes commands in order, waits for each.

Background (&) — Runs commands asynchronously.

Prints [job_number] pid.

Reports completion status:

Normal: command_name with pid <pid> exited normally

Abnormal: command_name with pid <pid> exited abnormally

MORE FEATURES

NOTE: regular shell commands like sleep, echo, grep, and anything else will run normally.

activities — Lists active/stopped background processes.
Displays: [pid] : command_name - State
Sorted lexicographically by command name.

ping — Sends signals to processes.
Syntax: ping <pid> <signal_number>

If invalid → Invalid syntax!

If process missing → No such process found

Success → Sent signal <num> to process with pid <pid>

Keyboard Shortcuts:

Ctrl-C (SIGINT): Sends interrupt to foreground process.

Ctrl-D (EOF): Kills all child processes → prints logout and exits.

Ctrl-Z (SIGTSTP): Stops current process → [job_number] Stopped command_name.

fg / bg:

fg [job] — Bring job to foreground.

bg [job] — Resume stopped job in background.

Handles missing job number and invalid job cases gracefully.

EXAMPLE SESSION

<user@system:~> hop ..
<user@system:/home> reveal
projects
<user@system:/home> hop projects/myshell
<user@system:~/projects/myshell> reveal -la
.git
.gitignore
Makefile
README.md
include
src
shell.out
<user@system:~/projects/myshell> sleep 10 &
[1] 4521
<user@system:~/projects/myshell> activities
[4521] : sleep - Running

