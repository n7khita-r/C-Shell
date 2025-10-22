#include "shell.h"
#include <string.h>

// Check if a command is an intrinsic command
int is_intrinsic_command(const char *command) {
    if (!command) {
        return 0;
    }
    
    // List of all intrinsic commands
    if (strcmp(command, "hop") == 0) {
        return 1;
    }
    
    // Add more intrinsic commands here as they are implemented
    // if (strcmp(command, "reveal") == 0) return 1;
    // if (strcmp(command, "log") == 0) return 1;
    // if (strcmp(command, "proclore") == 0) return 1;
    // if (strcmp(command, "seek") == 0) return 1;
    
    return 0;
}

// Execute intrinsic command - dispatcher function
int execute_intrinsic(char **tokens, int token_count) {
    if (token_count == 0 || !tokens[0]) {
        return 1;
    }
    
    // Dispatch to appropriate intrinsic command handler
    if (strcmp(tokens[0], "hop") == 0) {
        return hop(tokens, token_count);
    }
    
    // Add more intrinsic command handlers here
    /*
    if (strcmp(tokens[0], "reveal") == 0) {
        return execute_reveal(&tokens[1], token_count - 1);
    }
    
    if (strcmp(tokens[0], "log") == 0) {
        return execute_log(&tokens[1], token_count - 1);
    }
    
    if (strcmp(tokens[0], "proclore") == 0) {
        return execute_proclore(&tokens[1], token_count - 1);
    }
    
    if (strcmp(tokens[0], "seek") == 0) {
        return execute_seek(&tokens[1], token_count - 1);
    }
    */
    
    return 1; // Unknown intrinsic command
}

// Cleanup all intrinsic commands
void cleanup_intrinsics(void) {
    // Call cleanup functions for all intrinsic commands
    cleanup_hop();
    
    // Add cleanup calls for other intrinsic commands as needed
    // cleanup_reveal();
    // cleanup_log();
    // cleanup_proclore();
    // cleanup_seek();
}
