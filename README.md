# C Shell

> A **POSIX-compliant** custom shell implementation in C for a course project, featuring interactive prompts, sophisticated command parsing, built-in utilities, pipes, I/O redirection, and comprehensive process control.

---

## ⚡ Quick Start

### Compilation

Navigate to the project directory and execute:

```bash
make clean
make all
```

---

## 🎯 Features at a Glance

### **Interactive Shell Prompt**
```
<Username@SystemName:current_path>
```
- Displays `~` for home-relative paths
- Line-by-line command input
- Grammar-aware parsing with syntax validation
- Invalid syntax detection → `Invalid Syntax!`

---

## 🛠️ Built-in Commands

### `hop` — Smart Directory Navigation
Change directories with intelligent path handling:

- `hop ~` — Jump to home directory
- `hop .` — Current directory
- `hop ..` — Parent directory
- `hop -` — Previous directory
- `hop <path>` — Named directory

**Error Handling:**
- Invalid paths → `No such directory!`

---

### `reveal` — Enhanced Directory Listing
List directory contents with powerful flags:

**Syntax:**
```bash
reveal [flags] [path]
```

**Flags:**
- `-a` — Show hidden files
- `-l` — Long format with details
- Combined: `-la`, `-al`

**Features:**
- Lexicographic sorting
- Hidden files (starting with `.`) shown with `-a` flag

**Error Handling:**
- Too many arguments → `reveal: Invalid Syntax!`
- Missing previous directory → `No such directory!`

---

### `log` — Persistent Command History
Maintains a rolling history of the last **15 commands** across sessions.

**Features:**
- Persists between shell sessions
- Ignores consecutive duplicates
- Excludes `log` commands from history

**Subcommands:**

| Command | Description |
|---------|-------------|
| `log` | Display command history |
| `log purge` | Clear all history |
| `log execute <index>` | Re-run command by index |

---

## 🔀 Advanced I/O Operations

### **Input Redirection**
```bash
command < input_file
```
- Reads STDIN from specified file
- Error: Missing file → `No such file or directory`

### **Output Redirection**
```bash
command > output_file   # Truncate and write
command >> output_file  # Append mode
```
- Error: Creation failure → `Unable to create file for writing`

### **Pipes**
Chain commands seamlessly:
```bash
command1 | command2 | command3
```
- Supports mixing with redirections
- Multi-stage data processing

---

## ⚙️ Process Control

### **Execution Modes**

#### Sequential Execution (`;`)
```bash
cmd1 ; cmd2 ; cmd3
```
Executes commands in order, waiting for each to complete.

#### Background Execution (`&`)
```bash
long_running_task &
```
- Runs command asynchronously
- Prints: `[job_number] pid`

**Completion Notifications:**
- Normal exit: `command_name with pid <pid> exited normally`
- Abnormal exit: `command_name with pid <pid> exited abnormally`

---

### `activities` — Process Monitor
Lists all active and stopped background processes:
```bash
activities
```
**Output Format:**
```
[pid] : command_name - State
```
- Sorted lexicographically by command name

---

### `ping` — Signal Dispatcher
Send signals to processes:
```bash
ping <pid> <signal_number>
```

**Responses:**
- Invalid syntax → `Invalid syntax!`
- Process not found → `No such process found`
- Success → `Sent signal <num> to process with pid <pid>`

---

### `fg` / `bg` — Job Control
Manage background and foreground processes:

```bash
fg [job_number]  # Bring job to foreground
bg [job_number]  # Resume stopped job in background
```

**Error Handling:**
- Handles missing job numbers
- Validates job existence

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Signal | Action |
|----------|--------|--------|
| `Ctrl-C` | `SIGINT` | Interrupt foreground process |
| `Ctrl-D` | `EOF` | Kill all children, print `logout`, exit shell |
| `Ctrl-Z` | `SIGTSTP` | Stop current process → `[job_number] Stopped command_name` |

---

## 💻 Example Session

```bash
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
[1] 12345

<user@system:~/projects/myshell> echo "Building..." > build.log
<user@system:~/projects/myshell> cat build.log
Building...

<user@system:~/projects/myshell> ls | grep ".c" | wc -l
5

[1] sleep with pid 12345 exited normally

<user@system:~/projects/myshell> activities
[12346] : gcc - Running
[12347] : vim - Stopped

<user@system:~/projects/myshell> log
1. hop ..
2. reveal
3. hop projects/myshell
4. reveal -la
5. sleep 10 &

<user@system:~/projects/myshell> log execute 2
projects

<user@system:~/projects/myshell> ^D
logout
```

---

## 🚀 Additional Features

### **Standard Command Support**
All regular shell commands work seamlessly:
- `sleep`, `echo`, `grep`, `cat`, `ls`, `wc`, `find`, etc.
- Full integration with system binaries

### **Robust Error Handling**
- Graceful handling of edge cases
- Informative error messages
- Syntax validation at parse time

### **Session Persistence**
- Command history survives shell restarts
- Maintains context across sessions

---

## 📋 Technical Specifications

- **Language:** C
- **Standards:** POSIX-compliant
- **Architecture:** Modular design with separate parsing, execution, and I/O handling
- **History Size:** 15 commands (rolling window)
- **Process Management:** Full job control with signal handling

---

**Built with ❤️ using C and a deep appreciation for Unix philosophy**
