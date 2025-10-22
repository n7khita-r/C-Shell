#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include "shell.h"

#ifndef PATH_MAX
#ifdef _POSIX_PATH_MAX
#define PATH_MAX _POSIX_PATH_MAX
#else
#define PATH_MAX 4096
#endif
#endif
// change the shell's current working directory :)
// Global variable to track if hop has been called (defined in main shell file)
extern int hop_called;

void hop(int argc, char **argv) {
    char cwd[PATH_MAX];
    
    // Get current working directory
    if (!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "getcwd: %s\n", strerror(errno));
        return;
    }
    
    // If no arguments, go to home directory
    if (argc == 1) {
        const char *home = getenv("HOME");
        if (!home) {
            home = cwd; // Stay in current directory if HOME not set
        }
        
        if (strcmp(home, cwd) != 0) { // Only change if different
            if (chdir(home) != 0) {
                if (errno == ENOENT) {
                    fprintf(stderr, "No such directory!\n");
                } else {
                    fprintf(stderr, "hop: %s\n", strerror(errno));
                }
                return;
            }
            // Update OLDPWD only if we successfully changed directory
            setenv("OLDPWD", cwd, 1);
            hop_called = 1;
        }
        return;
    }
    
    // Process each argument sequentially
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        
        // Get current directory before each operation
        if (!getcwd(cwd, sizeof(cwd))) {
            fprintf(stderr, "getcwd: %s\n", strerror(errno));
            continue;
        }
        
        if (strcmp(arg, "~") == 0) {
            // Go to home directory
            const char *home = getenv("HOME");
            if (!home) {
                continue; // Do nothing if HOME not set
            }
            
            if (strcmp(home, cwd) != 0) { // Only change if different
                if (chdir(home) != 0) {
                    if (errno == ENOENT) {
                        fprintf(stderr, "No such directory!\n");
                    } else {
                        fprintf(stderr, "hop: %s\n", strerror(errno));
                    }
                    continue;
                }
                // Update OLDPWD only if we successfully changed directory
                setenv("OLDPWD", cwd, 1);
                hop_called = 1;
            }
            
        } else if (strcmp(arg, ".") == 0) {
            // Do nothing (stay in same directory)
            continue;
            
        } else if (strcmp(arg, "..") == 0) {
            // Go to parent directory
            char parent[PATH_MAX];
            if (chdir("..") == 0) {
                if (getcwd(parent, sizeof(parent))) {
                    // Check if we actually moved (not already at root)
                    if (strcmp(parent, cwd) != 0) {
                        setenv("OLDPWD", cwd, 1);
                        hop_called = 1;
                    } else {
                        // We're at root, no change happened, go back
                        chdir(cwd);
                    }
                } else {
                    // Failed to get new cwd, go back to original
                    chdir(cwd);
                }
            } else {
                if (errno == ENOENT) {
                    fprintf(stderr, "No such directory!\n");
                } else {
                    fprintf(stderr, "hop: %s\n", strerror(errno));
                }
            }
            
        } else if (strcmp(arg, "-") == 0) {
            const char *oldpwd = getenv("OLDPWD");
            if (!oldpwd || strlen(oldpwd) == 0) {
                // No previous directory exists
                continue;
            }

            if (chdir(oldpwd) != 0) {
                if (errno == ENOENT) {
                    fprintf(stderr, "No such directory!\n");
                } else {
                    fprintf(stderr, "hop: %s\n", strerror(errno));
                }
                continue;
            }

            // Save current directory before updating OLDPWD
            char temp_cwd[PATH_MAX];
            if (!getcwd(temp_cwd, sizeof(temp_cwd))) {
                fprintf(stderr, "getcwd: %s\n", strerror(errno));
                continue;
            }

            // Update OLDPWD to the directory we just left
            setenv("OLDPWD", cwd, 1);
            
            // Update PWD to reflect new location
            setenv("PWD", temp_cwd, 1);
            
            // Ensure hop_called is set to maintain history
            hop_called = 1;
            
        } else {
            // Regular path (relative or absolute)
            if (chdir(arg) != 0) {
                if (errno == ENOENT) {
                    fprintf(stderr, "No such directory!\n");
                } else {
                    fprintf(stderr, "hop: %s\n", strerror(errno));
                }
                continue;
            }
            
            // Update OLDPWD only if we successfully changed directory
            setenv("OLDPWD", cwd, 1);
            hop_called = 1;
        }
        
        // Update PWD after successful directory change
        char new_cwd[PATH_MAX];
        if (getcwd(new_cwd, sizeof(new_cwd))) {
            setenv("PWD", new_cwd, 1);
        }
    }
    
    // Flush any output (though hop typically doesn't output much)
    fflush(stdout);
    fflush(stderr);
}
