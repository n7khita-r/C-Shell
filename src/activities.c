#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

// Structure to hold process information for sorting
typedef struct {
    pid_t pid;
    char command_name[256];
    char state[32];
} process_info_t;

// Compare function for qsort (lexicographical by command name)
static int compare_processes(const void *a, const void *b) {
    const process_info_t *proc_a = (const process_info_t *)a;
    const process_info_t *proc_b = (const process_info_t *)b;
    return strcmp(proc_a->command_name, proc_b->command_name);
}

// Check if a process is still running
static int is_process_running(pid_t pid) {
    if (kill(pid, 0) == 0) {
        // Process exists, check if it's stopped
        int status;
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == 0) {
            // Process is still running
            return 1;
        } else if (result > 0 && WIFSTOPPED(status)) {
            // Process is stopped
            return 2;
        } else {
            // Process has terminated
            return 0;
        }
    }
    return 0; // Process doesn't exist
}

void activities(int argc, char **argv) {
    (void)argc; // Suppress unused parameter warning
    (void)argv;
    
    // Get the list of background jobs from bg_jobs
    extern int get_active_job_count(void);
    extern void get_job_info(int index, pid_t *pid, char *command, int *status);
    
    int job_count = get_active_job_count();
    
    if (job_count == 0) {
       // printf("No background processes running.\n");
        return;
    }
    
    // Collect process information
    process_info_t *processes = malloc(job_count * sizeof(process_info_t));
    if (!processes) {
        perror("malloc");
        return;
    }
    
    int valid_count = 0;
    
    // Check each job and collect valid ones
    for (int i = 0; i < job_count; i++) {
        pid_t pid;
        char command[256];
        int status;
        
        get_job_info(i, &pid, command, &status);
        
        if (pid > 0) {
            int running_state = is_process_running(pid);
            
            if (running_state > 0) {
                // Process is still active
                processes[valid_count].pid = pid;
                strncpy(processes[valid_count].command_name, command, 255);
                processes[valid_count].command_name[255] = '\0';
                
                if (running_state == 1) {
                    strcpy(processes[valid_count].state, "Running");
                } else {
                    strcpy(processes[valid_count].state, "Stopped");
                }
                
                valid_count++;
            }
        }
    }
    
    if (valid_count == 0) {
        printf("No background processes running.\n");
        free(processes);
        return;
    }
    
    // Sort processes lexicographically by command name
    qsort(processes, valid_count, sizeof(process_info_t), compare_processes);
    
    // Display processes in required format: [pid] : command_name - State
    for (int i = 0; i < valid_count; i++) {
        printf("[%d] : %s - %s\n", 
               processes[i].pid, 
               processes[i].command_name, 
               processes[i].state);
    }
    
    free(processes);
} 