#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <string.h>

#include "bg_jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <termios.h>

// Global variables for background job tracking
static bg_job_t bg_jobs[MAX_JOBS];
static int next_job_id = 1;
static int job_count = 0;

// Global variables for signal handling
pid_t foreground_pid = 0;
pid_t foreground_pgid = 0;

// Initialize background job tracking system
void init_bg_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        bg_jobs[i].active = 0;
        bg_jobs[i].job_id = 0;
        bg_jobs[i].pid = 0;
        bg_jobs[i].pgid = 0;
        bg_jobs[i].status = JOB_RUNNING;
        memset(bg_jobs[i].command, 0, MAX_CMD_LEN);
    }
    next_job_id = 1;
    job_count = 0;
    foreground_pid = 0;
    foreground_pgid = 0;
}

// Check for completed background processes (non-blocking)
void check_background_processes(void) {
    pid_t pid;
    int status;
    
    // Check all background processes without blocking
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        // Find the job with this PID
        for (int i = 0; i < MAX_JOBS; i++) {
            if (bg_jobs[i].active && bg_jobs[i].pid == pid) {
                if (WIFSTOPPED(status)) {
                    // Process was stopped (Ctrl-Z)
                    bg_jobs[i].status = JOB_STOPPED;
                    printf("[%d] Stopped %s\n", bg_jobs[i].job_id, bg_jobs[i].command);
                    fflush(stdout);
                } else if (WIFCONTINUED(status)) {
                    bg_jobs[i].status = JOB_RUNNING;
                    printf("[%d] Continued %s\n", bg_jobs[i].job_id, bg_jobs[i].command);
                    fflush(stdout);
                } else {
                    // Process completed - print completion message to stdout
                    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                        printf("%s with pid %d exited normally\n", 
                               bg_jobs[i].command, pid);
                    } else {
                        printf("%s with pid %d exited abnormally\n", 
                               bg_jobs[i].command, pid);
                    }
                    fflush(stdout);
                    
                    // Remove from job list
                    bg_jobs[i].active = 0;
                    bg_jobs[i].job_id = 0;
                    bg_jobs[i].pid = 0;
                    bg_jobs[i].pgid = 0;
                    bg_jobs[i].status = JOB_DONE;
                    memset(bg_jobs[i].command, 0, MAX_CMD_LEN);
                    job_count--;
                }
                break;
            }
        }
    }
}

// Add a new background job to tracking
int add_background_job(pid_t pid, const char* command) {
    if (!command || pid <= 0) {
        return -1;
    }
    
    // Find an empty slot
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!bg_jobs[i].active) {
            bg_jobs[i].job_id = next_job_id++;
            bg_jobs[i].pid = pid;
            bg_jobs[i].pgid = pid; // Usually the same as pid for background jobs
            bg_jobs[i].status = JOB_RUNNING;
            
            // Store just the command name, not full path with args
            strncpy(bg_jobs[i].command, command, MAX_CMD_LEN - 1);
            bg_jobs[i].command[MAX_CMD_LEN - 1] = '\0';
            
            bg_jobs[i].active = 1;
            job_count++;
            
            // Print job information to stderr as required
            fprintf(stderr, "[%d] %d\n", bg_jobs[i].job_id, pid);
            fflush(stderr);
            
            return bg_jobs[i].job_id;
        }
    }
    
    return -1; // No space for new job
}

// Remove a background job (called when job completes)
void remove_background_job(pid_t pid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (bg_jobs[i].active && bg_jobs[i].pid == pid) {
            bg_jobs[i].active = 0;
            bg_jobs[i].job_id = 0;
            bg_jobs[i].pid = 0;
            memset(bg_jobs[i].command, 0, MAX_CMD_LEN);
            job_count--;
            break;
        }
    }
}

// Check if input contains '&' (background operator)
int has_background_ampersand(const char* input) {
    if (!input) return 0;
    
    // Look for '&' anywhere in the string (not just at the end)
    return strchr(input, '&') != NULL;
}
// ==================================== LLM GENERATED CODE BEGINS ======================================
// Split input on '&' and return array of command parts
// Caller must free the returned array and its elements
char** split_on_ampersand(const char* input, int* count) {
    if (!input || !count) {
        if (count) *count = 0;
        return NULL;
    }
    
    *count = 0;
    
    // Count '&' characters to determine array size
    int amp_count = 0;
    for (int i = 0; input[i]; i++) {
        if (input[i] == '&') amp_count++;
    }
    
    // We'll have amp_count + 1 parts
    int parts_count = amp_count + 1;
    char** parts = malloc(sizeof(char*) * (parts_count + 1)); // +1 for NULL terminator
    if (!parts) return NULL;
    
    // Initialize all to NULL
    for (int i = 0; i <= parts_count; i++) {
        parts[i] = NULL;
    }
    
    // Split the string
    char* input_copy = strdup(input);
    if (!input_copy) {
        free(parts);
        return NULL;
    }
    
    char* token = strtok(input_copy, "&");
    int index = 0;
    
    while (token != NULL && index < parts_count) {
        // Trim whitespace from token
        while (*token && isspace(*token)) token++;
        if (*token) {
            char* end = token + strlen(token) - 1;
            while (end > token && isspace(*end)) {
                *end = '\0';
                end--;
            }
            
            if (*token) {  // Only add non-empty tokens
                parts[index] = strdup(token);
                if (!parts[index]) {
                    // Cleanup on error
                    for (int i = 0; i < index; i++) {
                        free(parts[i]);
                    }
                    free(parts);
                    free(input_copy);
                    return NULL;
                }
                index++;
            }
        }
        token = strtok(NULL, "&");
    }
    
    *count = index;
    free(input_copy);
    return parts;
}

// ================================ LLM GENERATED CODE ENDS ========================================================

// Free array returned by split_on_ampersand
void free_string_array(char** arr) {
    if (!arr) return;
    
    for (int i = 0; arr[i] != NULL; i++) {
        free(arr[i]);
    }
    free(arr);
}

// Remove trailing '&' and return a new string (caller must free)
char* remove_trailing_ampersand(const char* input) {
    if (!input) return NULL;
    
    int len = strlen(input);
    if (len == 0) return strdup("");
    
    // Find the last '&'
    int amp_pos = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (input[i] == '&') {
            amp_pos = i;
            break;
        } else if (!isspace(input[i])) {
            break;
        }
    }
    
    if (amp_pos == -1) {
        // No '&' found
        return strdup(input);
    }
    
    // Create new string without the '&' and trailing whitespace
    char* result = malloc(amp_pos + 1);
    if (!result) return NULL;
    
    strncpy(result, input, amp_pos);
    result[amp_pos] = '\0';
    
    // Trim trailing whitespace
    int end = amp_pos - 1;
    while (end >= 0 && isspace(result[end])) {
        result[end] = '\0';
        end--;
    }
    
    return result;
}

// Get number of active background jobs
int get_active_job_count(void) {
    return job_count;
}

// Get job information by index (for activities command)
void get_job_info(int index, pid_t *pid, char *command, int *status) {
    if (index < 0 || index >= MAX_JOBS || !pid || !command || !status) {
        if (pid) *pid = 0;
        if (command) command[0] = '\0';
        if (status) *status = JOB_DONE;
        return;
    }
    
    if (bg_jobs[index].active) {
        *pid = bg_jobs[index].pid;
        strncpy(command, bg_jobs[index].command, MAX_CMD_LEN - 1);
        command[MAX_CMD_LEN - 1] = '\0';
        *status = bg_jobs[index].status;
    } else {
        *pid = 0;
        command[0] = '\0';
        *status = JOB_DONE;
    }
}

// Find job by job number (1-based)
int find_job_by_number(int job_number) {
    if (job_number <= 0) return -1;
    
    for (int i = 0; i < MAX_JOBS; i++) {
        if (bg_jobs[i].active && bg_jobs[i].job_id == job_number) {
            return i;
        }
    }
    return -1;
}

// Get the most recent background/stopped job
int get_most_recent_job(void) {
    int most_recent = -1;
    int highest_id = -1;
    
    for (int i = 0; i < MAX_JOBS; i++) {
        if (bg_jobs[i].active && bg_jobs[i].job_id > highest_id) {
            highest_id = bg_jobs[i].job_id;
            most_recent = i;
        }
    }
    
    return most_recent;
}

// Get job number (ID) for a given job index
int get_job_number(int job_index) {
    if (job_index < 0 || job_index >= MAX_JOBS || !bg_jobs[job_index].active) {
        return -1;
    }
    return bg_jobs[job_index].job_id;
}

// Resume a stopped job (send SIGCONT)
void resume_job(int job_index) {
    if (job_index < 0 || job_index >= MAX_JOBS || !bg_jobs[job_index].active) {
        return;
    }
    
    if (bg_jobs[job_index].status == JOB_STOPPED) {
        if (kill(bg_jobs[job_index].pid, SIGCONT) == 0) {
            bg_jobs[job_index].status = JOB_RUNNING;
        }
    }
}

// Bring a job to foreground
void bring_job_to_foreground(int job_index) {
    if (job_index < 0 || job_index >= MAX_JOBS || !bg_jobs[job_index].active) {
        return;
    }

    // Save the shell's current controlling terminal foreground pgid
    pid_t shell_tty_pgid = tcgetpgrp(STDIN_FILENO);

    // Give terminal control to the job's process group
    setpgid(bg_jobs[job_index].pid, bg_jobs[job_index].pid);
    tcsetpgrp(STDIN_FILENO, bg_jobs[job_index].pid);

    // Track the foreground job for signal forwarding
    set_foreground_process(bg_jobs[job_index].pid, bg_jobs[job_index].pid);

    // Resume if stopped
    if (bg_jobs[job_index].status == JOB_STOPPED) {
        resume_job(job_index);
    }

    // Wait until the job completes or stops again
    int status;
    pid_t result = waitpid(bg_jobs[job_index].pid, &status, WUNTRACED);

    if (result > 0) {
        if (WIFEXITED(status)) {
            printf("%s with pid %d exited normally\n",
                   bg_jobs[job_index].command, bg_jobs[job_index].pid);
            remove_background_job(bg_jobs[job_index].pid);
        } else if (WIFSIGNALED(status)) {
            printf("%s with pid %d exited abnormally\n",
                   bg_jobs[job_index].command, bg_jobs[job_index].pid);
            remove_background_job(bg_jobs[job_index].pid);
        } else if (WIFSTOPPED(status)) {
            // Job was stopped again - mark as stopped and leave in job list
            bg_jobs[job_index].status = JOB_STOPPED;
            printf("[%d] Stopped %s\n", bg_jobs[job_index].job_id, bg_jobs[job_index].command);
        }
    }

    // Clear foreground tracking and restore terminal control
    clear_foreground_process();
    tcsetpgrp(STDIN_FILENO, shell_tty_pgid);
    fflush(stdout);
}

// Clean up all background jobs (for shell exit - Ctrl-D requirement)
void cleanup_all_jobs(void) {
    // Send SIGKILL to all active background processes (Ctrl-D requirement)
    for (int i = 0; i < MAX_JOBS; i++) {
        if (bg_jobs[i].active) {
            // Send SIGKILL to the entire process group
            kill(-bg_jobs[i].pgid, SIGKILL);
        }
    }
    
    // Reset the job tracking
    init_bg_jobs();
}

// Signal handler for SIGINT (Ctrl-C)
void sigint_handler(int sig) {
    (void)sig; // Suppress unused parameter warning
    
    if (foreground_pgid > 0) {
        // Send SIGINT to the foreground process group
        kill(-foreground_pgid, SIGINT);
    }
    // Shell itself doesn't terminate - just print newline and return
    write(STDOUT_FILENO, "\n", 1);
}

// Signal handler for SIGTSTP (Ctrl-Z)
void sigtstp_handler(int sig) {
    (void)sig; // Suppress unused parameter warning
    
    if (foreground_pgid > 0) {
        // Send SIGTSTP to the foreground process group
        kill(-foreground_pgid, SIGTSTP);
    }
    // Shell itself doesn't stop - just print newline
    write(STDOUT_FILENO, "\n", 1);
}

// Setup signal handling for the shell
void setup_signal_handling(void) {
    struct sigaction sa_int, sa_tstp;
    
    // Setup SIGINT handler (Ctrl-C)
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, NULL);
    
    // Setup SIGTSTP handler (Ctrl-Z)
    sa_tstp.sa_handler = sigtstp_handler;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, NULL);
    
    // Ignore SIGCHLD to prevent default behavior but handle with waitpid
    signal(SIGCHLD, SIG_DFL);
}

// Set the current foreground process
void set_foreground_process(pid_t pid, pid_t pgid) {
    foreground_pid = pid;
    foreground_pgid = pgid;
}

// Clear the current foreground process
void clear_foreground_process(void) {
    foreground_pid = 0;
    foreground_pgid = 0;
}

// Add a stopped job to the background job list
int add_stopped_job(pid_t pid, const char* command) {
    if (!command || pid <= 0) {
        return -1;
    }
    
    // Find an empty slot
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!bg_jobs[i].active) {
            bg_jobs[i].job_id = next_job_id++;
            bg_jobs[i].pid = pid;
            bg_jobs[i].pgid = pid;
            bg_jobs[i].status = JOB_STOPPED;
            
            strncpy(bg_jobs[i].command, command, MAX_CMD_LEN - 1);
            bg_jobs[i].command[MAX_CMD_LEN - 1] = '\0';
            
            bg_jobs[i].active = 1;
            job_count++;
            
            return bg_jobs[i].job_id;
        }
    }
    
    return -1;
}