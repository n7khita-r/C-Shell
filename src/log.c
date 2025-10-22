#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_COMMANDS 15 // keeps only last 15 commands
#define LOG_FILE ".shell_history" // load from .shell_history file on startup
#define MAX_CMD_LENGTH 1024

// Global command history
static char command_history[MAX_COMMANDS][MAX_CMD_LENGTH];
static int history_count = 0; // no of valid entries in buffer
static int history_start = 0; // Index of oldest command
static int skip_history = 0;  // Flag to skip adding to history
// circular buffer
// Load command history from file
void load_history(void) {
    FILE *file = fopen(LOG_FILE, "r");
    if (!file) {
        return; // No history file exists yet
    }
    
    char line[MAX_CMD_LENGTH];
    history_count = 0;
    history_start = 0;
    
    while (fgets(line, sizeof(line), file) && history_count < MAX_COMMANDS) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip empty lines
        if (strlen(line) == 0) {
            continue;
        }
        
        strcpy(command_history[history_count], line);
        history_count++;
    }
    
    fclose(file);
}

// Save command history to file
static void save_history(void) {
    FILE *file = fopen(LOG_FILE, "w");
    if (!file) {
        return; // Could not create history file
    }
    
    // Write commands from oldest to newest
    for (int i = 0; i < history_count; i++) {
        int idx = (history_start + i) % MAX_COMMANDS;
        fprintf(file, "%s\n", command_history[idx]);
    }
    
    fclose(file);
}

// Add command to history
void add_to_history(const char *command) {
    // Check skip flag first (for executed commands from history)
    if (skip_history) {
        return;
    }
    
    // Don't store log commands (requirement #5)
    if (strncmp(command, "log", 3) == 0 && 
        (command[3] == '\0' || command[3] == ' ')) {
        return;
    }
    
    // Skip if command is identical to the last one (requirement #3)
    if (history_count > 0) {
        int last_idx = (history_start + history_count - 1) % MAX_COMMANDS;
        if (strcmp(command_history[last_idx], command) == 0) {
            return; // Skip duplicate
        }
    }
    
    // Determine where to place the new command (requirement #2 - circular buffer)
    int new_idx;
    if (history_count < MAX_COMMANDS) {
        // Still have room, just add to the end
        new_idx = history_count;
        history_count++;
    } else {
        // Buffer is full, overwrite the oldest
        new_idx = history_start;
        history_start = (history_start + 1) % MAX_COMMANDS;
    }
    
    // Store the command (requirement #4 - entire shell_cmd)
    strncpy(command_history[new_idx], command, MAX_CMD_LENGTH - 1);
    command_history[new_idx][MAX_CMD_LENGTH - 1] = '\0';
    
    // Save to file after each addition (requirement #1 - persistence)
    save_history();
}

// Print command history (requirement #6a - oldest to newest)
static void print_history(void) {
    if (history_count == 0) {
        fflush(stdout); // Ensure output is flushed even when empty
        return; // No history to show
    }
    
    // Print from oldest to newest
    for (int i = 0; i < history_count; i++) {
        int idx = (history_start + i) % MAX_COMMANDS;
        printf("%s\n", command_history[idx]);
    }
    fflush(stdout); // Pipeline compatibility
}

// Execute command at given index (requirement #6c)
static void execute_at_index(int index) {
    if (index < 1 || index > history_count) {
        fprintf(stderr, "Invalid index: %d\n", index); // Error to stderr
        return;
    }
    
    // Convert from one-indexed (newest first) to array index
    // Index 1 = newest command, Index history_count = oldest command
    int array_idx = (history_start + history_count - index) % MAX_COMMANDS;
    char command_copy[MAX_CMD_LENGTH];
    strncpy(command_copy, command_history[array_idx], MAX_CMD_LENGTH - 1);
    command_copy[MAX_CMD_LENGTH - 1] = '\0';
    
    // Print the command being executed (as shown in example)
    printf("%s\n", command_copy);
    fflush(stdout); // Pipeline compatibility
    
    // Set flag to prevent executed command from being added to history again
    skip_history = 1;
    // Execute the command without adding it to history (requirement #6c)
    execute_command(command_copy);
    // Reset flag
    skip_history = 0;
}

// Clear command history (requirement #6b)
static void purge_history(void) {
    history_count = 0;
    history_start = 0;
    
    // Remove history file
    unlink(LOG_FILE);
    fflush(stdout); // Pipeline compatibility
}

void log_command(int argc, char **argv) {
    // Initialize history on first call
    static int initialized = 0;
    if (!initialized) {
        load_history();
        initialized = 1;
    }
    
    if (argc == 1) {
        // No arguments - print history (requirement #6a)
        print_history();
    } else if (argc == 2 && strcmp(argv[1], "purge") == 0) {
        // Purge history (requirement #6b)
        purge_history();
    } else if (argc == 3 && strcmp(argv[1], "execute") == 0) {
        // Execute command at index (requirement #6c)
        int index = atoi(argv[2]);
        execute_at_index(index);
    } else {
        fprintf(stderr, "Usage: log [purge | execute <index>]\n"); // Error to stderr
    }
}