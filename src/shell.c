#include "shell.h"
#include "bg_jobs.h"
#include <signal.h> // handles ctrl c ctrl d ctrl z etc.
#include <termios.h>
#include <pwd.h>
#include <sys/types.h>
// main program for shell
static char *home_directory = NULL; // home dir
int hop_called = 0; // we can't do hop - without alr calling hop so counter
int main(void) {
// signal(SIGTSTP, SIG_IGN);
signal(SIGTTIN, SIG_IGN); // sent when bg process tries to r/w to terminal
signal(SIGTTOU, SIG_IGN); // are ignored so shell remains in control of terminal i/o + does not stop
char input[MAX_INPUT_SIZE]; // input buffer 
char spawn_cwd[PATH_MAX]; // working dir current
if (getcwd(spawn_cwd, sizeof(spawn_cwd))) {
 home_directory = strdup(spawn_cwd);
setenv("HOME", spawn_cwd, 1);
setenv("PWD", spawn_cwd, 1);
setenv("OLDPWD", spawn_cwd, 1);
 } else {
 home_directory = get_home_directory();
if (!getenv("HOME")) {
struct passwd *pw = getpwuid(getuid());
if (pw && pw->pw_dir && pw->pw_dir[0] != '\0') {
setenv("HOME", pw->pw_dir, 1);
 }
 }
char cwd_buf[PATH_MAX];
if (getcwd(cwd_buf, sizeof(cwd_buf))) {
if (!getenv("PWD")) setenv("PWD", cwd_buf, 1);
if (!getenv("OLDPWD")) setenv("OLDPWD", cwd_buf, 1);
 }
 } // gets all env variables regarding path and all
init_bg_jobs(); 
load_history(); // loads history (15 commands consistently stored accross all sessions)
setup_signal_handling();
setpgid(0, 0); // puts shell process in its own process group
tcsetpgrp(STDIN_FILENO, getpgrp()); // makes the shell process grp the foreground process grp for terminal
while (1) {
check_background_processes(); // cleans up bg processes
display_prompt(); // displays prompt
// Handle EOF (Ctrl-D) detection
if (get_user_input(input) == -1) {
// fprintf(stderr, "DEBUG: EOF detected, printing logout\n"); // Debug to stderr
printf("logout\n"); // logout on ctrl D
fflush(stdout); // cleanup
fflush(stderr);
cleanup_all_jobs(); // cleanup
if (home_directory) {
free(home_directory);
 } // free placeholder
exit(0); // exit
}
// Trim whitespace from input
trim_whitespace(input);
// Skip empty input
if (strlen(input) == 0) {
continue;
 }
// Parse and validate command
if (!parse_command(input)) {
printf("Invalid Syntax!\n"); // put through parser and check for syntax
add_to_history(input);
 } else {
// Execute the command (now background-aware with signal handling)
execute_command(input); // execute
// Reap and print any background completions immediately
check_background_processes(); // reaps bg processes - Double-checking ensures your shell updates job statuses as soon as commands finish.
fflush(stdout);
 }
 }
// This should never be reached, but cleanup just in case
cleanup_all_jobs();
if (home_directory) {
free(home_directory);
 }
return 0;
}
// worst case -- if loop breaks and all