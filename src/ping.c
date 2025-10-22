#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

void ping(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: ping <pid> <signal_number>\n");
        return;
    } // checks syntax ping pid signal number
    
    // Parse PID
    char *endptr;
    long pid_long = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || pid_long <= 0) {
        printf("Invalid PID: %s\n", argv[1]);
        return;
    }
    pid_t pid = (pid_t)pid_long;
    
    // Parse signal number
    long signal_long = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0') {
        printf("Invalid signal number: %s\n", argv[2]);
        return;
    }

    // converts string into a long int endptr detects invalid non numeric input
    
    // Take signal number modulo 32 as required
    int actual_signal = (int)(signal_long % 32);
    
    // Check if process exists by sending signal 0 (doesn't actually send a signal)
    if (kill(pid, 0) == -1) { // returns 0 if process exists and allows signalling and -1 if no process exists and sets errno
        if (errno == ESRCH) {
            printf("No such process found\n");
        } else {
            perror("ping");
        }
        return;
    }
    
    // Send the actual signal
    if (kill(pid, actual_signal) == -1) {
        perror("ping"); // permission denied
        return;
    }
    
    // Success message
    printf("Sent signal %d to process with pid %d\n", actual_signal, pid);
} 