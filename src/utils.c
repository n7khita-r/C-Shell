#include "shell.h"
// helper functions
void trim_whitespace(char *str) {
    if (!str) return;
    
    // Trim leading whitespace
    int start = 0;
    while (isspace(str[start])) {
        start++;
    }
    
    // Move string content
    if (start > 0) {
        memmove(str, str + start, strlen(str + start) + 1); // \0 copied too 
    }
    // memmove safely moves bytes 
    // Trim trailing whitespace
    int end = strlen(str) - 1;
    while (end >= 0 && isspace(str[end])) {
        str[end] = '\0';
        end--;
    } // replaces spaces with terminator until finds a nonspace character and trims 
}
// basically this implements the requirement that space has no effect on the cmd 

int skip_whitespace(const char *str, int start) {
    while (str[start] && isspace(str[start])) {
        start++;
    }
    return start;
}
// skips whitespace from a given index 
// Get home directory
char* get_home_directory(void) {
    char *home = getenv("HOME"); // finds env home for prompt and all
    if (home) {
        char *home_copy = malloc(strlen(home) + 1);
        if (home_copy) {
            strcpy(home_copy, home); // copies home path to another str
            return home_copy;
        }
    }
    return NULL; 
}

// Display shell prompt
void display_prompt(void) {
    char cwd[PATH_MAX]; // current working dir
    char hostname[256]; // system name (hp pav laptop)
    char *username = getenv("USER"); // user name
    char *home = getenv("HOME"); // home path
    char display_path[PATH_MAX]; // construct display path for prompt
    
    // Get current working directory
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strcpy(cwd, "unknown");
    }
    
    // Get hostname
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "unknown");
    }
    
    // Get username
    if (!username) {
        username = "unknown";
    }
    
    // Replace home directory with ~ in the path
    if (home && strncmp(cwd, home, strlen(home)) == 0) {
        if (strcmp(cwd, home) == 0) {
            // Exactly at home directory
            strcpy(display_path, "~");
        } else if (cwd[strlen(home)] == '/') {
            // In a subdirectory of home
            snprintf(display_path, sizeof(display_path), "~%s", cwd + strlen(home));
        } else {
            // Path starts with home but isn't actually under it
            strcpy(display_path, cwd);
        }
    } else {
        // Not in home directory, show full path
        strcpy(display_path, cwd);
    }
    
    // Print prompt in format: <Username@SystemName:current_path>
    printf("<%s@%s:%s> ", username, hostname, display_path);
    fflush(stdout);
}