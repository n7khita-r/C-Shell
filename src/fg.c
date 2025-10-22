#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void fg(int argc, char **argv) {
    int job_number = -1;
    
    if (argc == 1) {
        // No job number provided, use most recent job req 4
        extern int get_most_recent_job(void);
        int job_index = get_most_recent_job();
        
        if (job_index == -1) {
            printf("No background jobs\n");
            return;
        }
        
        // Get job info for display
        extern void get_job_info(int index, pid_t *pid, char *command, int *status);
        pid_t pid;
        char command[256];
        int status;
        get_job_info(job_index, &pid, command, &status);
        
        printf("%s\n", command);
        
        // Bring job to foreground
        extern void bring_job_to_foreground(int job_index);
        bring_job_to_foreground(job_index);
        
    } else if (argc == 2) {
        // Job number provided
        char *endptr;
        job_number = (int)strtol(argv[1], &endptr, 10);
        
        if (*endptr != '\0' || job_number <= 0) {
            printf("Invalid job number: %s\n", argv[1]);
            return;
        }
        
        // Find job by number
        extern int find_job_by_number(int job_number);
        int job_index = find_job_by_number(job_number);
        
        if (job_index == -1) {
            printf("No such job\n");
            return;
        }
        
        // Get job info for display
        extern void get_job_info(int index, pid_t *pid, char *command, int *status);
        pid_t pid;
        char command[256];
        int status;
        get_job_info(job_index, &pid, command, &status);
        
        printf("%s\n", command);
        
        // Bring job to foreground
        extern void bring_job_to_foreground(int job_index);
        bring_job_to_foreground(job_index);
        
    } else {
        printf("Usage: fg [job_number]\n");
    }
} 