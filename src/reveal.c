#include "shell.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <fnmatch.h>
#include <unistd.h>

#ifndef PATH_MAX
    #ifdef _POSIX_PATH_MAX
        #define PATH_MAX _POSIX_PATH_MAX
    #else
        #define PATH_MAX 4096
    #endif
#endif

// External variable from hop.c
extern int hop_called;
// we cannot run reveal - unless hop is called atleast once before
// Compare function for qsort (lexicographic order using ASCII values)
static int compare_strings(const void *a, const void *b) {
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    
    // Use strcmp for pure ASCII lexicographic comparison
    return strcmp(sa, sb);
}
// always listed lexicographical order
// Case-insensitive pattern matching function
static int match_pattern_case_insensitive(const char *pattern, const char *string) {
    // Convert both pattern and string to lowercase for comparison
    char *lower_pattern = malloc(strlen(pattern) + 1);
    char *lower_string = malloc(strlen(string) + 1);
    
    if (!lower_pattern || !lower_string) {
        free(lower_pattern);
        free(lower_string);
        return 0;
    }
    
    // Convert pattern to lowercase
    for (int i = 0; pattern[i]; i++) {
        lower_pattern[i] = tolower(pattern[i]);
    }
    lower_pattern[strlen(pattern)] = '\0';
    
    // Convert string to lowercase
    for (int i = 0; string[i]; i++) {
        lower_string[i] = tolower(string[i]);
    }
    lower_string[strlen(string)] = '\0';
    
    // Use fnmatch for pattern matching
    int result = fnmatch(lower_pattern, lower_string, 0) == 0;
    
    free(lower_pattern);
    free(lower_string);
    return result;
} // used for glob pattern matching like same way as grep "filename.txt"

// Check if a string contains glob characters
static int has_glob_chars(const char *str) {
    return strchr(str, '*') != NULL || strchr(str, '?') != NULL || strchr(str, '[') != NULL;
} // wildcard symbols

// Expand glob patterns in a directory
static char** expand_glob_pattern(const char *dir_path, const char *pattern, int *match_count) {
    *match_count = 0;
    
    DIR *dir = opendir(dir_path); // finds and opens dir
    if (!dir) {
        return NULL;
    }
    
    char **matches = NULL; 
    int capacity = 10; // initialization -- dw it will take more
    matches = malloc(capacity * sizeof(char*));
    if (!matches) {
        closedir(dir);
        return NULL;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and .. directories for glob patterns
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Check if filename matches pattern (case-insensitive)
        if (match_pattern_case_insensitive(pattern, entry->d_name)) {
            // Expand array if needed
            if (*match_count >= capacity) {
                capacity *= 2; // here expands !!
                char **new_matches = realloc(matches, capacity * sizeof(char*));
                if (!new_matches) {
                    for (int i = 0; i < *match_count; i++) free(matches[i]);
                    free(matches);
                    closedir(dir);
                    return NULL;
                }
                matches = new_matches;
            }
            
            matches[*match_count] = strdup(entry->d_name);
            if (!matches[*match_count]) {
                for (int i = 0; i < *match_count; i++) free(matches[i]);
                free(matches);
                closedir(dir);
                return NULL;
            }
            (*match_count)++;
        }
    }
    
    closedir(dir);
    
    // Sort matches
    if (*match_count > 0) {
        qsort(matches, *match_count, sizeof(char*), compare_strings);
    }
    
    return matches;
} // macthes pattern adds to array sorts sets match count returns
// opens directory
// reads every file - readdir

// Get target directory path based on arguments (fixed version)
static char* get_target_directory(int argc, char **argv, int start_idx) {
    static char result_path[PATH_MAX*2];
    char cwd[PATH_MAX]; // decides which dir to list based on flags
    
    // Get current working directory
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return NULL;
    }
    
    // No arguments - use current directory
    if (start_idx >= argc) {
        strcpy(result_path, cwd);
        return result_path;
    }
    
    // Handle special cases
    if (strcmp(argv[start_idx], "~") == 0) {
        char *home = getenv("HOME");
        if (home) {
            strcpy(result_path, home);
        } else {
            strcpy(result_path, cwd);
        }
        return result_path;
    } else if (strcmp(argv[start_idx], ".") == 0) {
        strcpy(result_path, cwd);
        return result_path;
    } else if (strcmp(argv[start_idx], "..") == 0) {
        // Get parent directory path
        char temp_path[PATH_MAX];
        strcpy(temp_path, cwd);
        char *last_slash = strrchr(temp_path, '/');
        if (last_slash != NULL && last_slash != temp_path) {
            // Not at root, can go to parent
            *last_slash = '\0';  // Truncate at last slash
            strcpy(result_path, temp_path);
        } else {
            // At root directory, parent is root itself
            strcpy(result_path, "/");
        }
        return result_path;
    } else if (strcmp(argv[start_idx], "-") == 0) {
        // Check if hop has been called in this shell session
        if (!hop_called) {
            return NULL; // This will trigger "No such directory!" error
        }
        
        // Use OLDPWD environment for previous directory
        const char *oldpwd = getenv("OLDPWD");
        if (oldpwd && strlen(oldpwd) > 0) {
            strcpy(result_path, oldpwd);
        } else {
            return NULL; // No valid previous directory
        }
        return result_path;
    } else {
        // Relative or absolute path
        // For absolute paths, use as-is
        if (argv[start_idx][0] == '/') {
            strcpy(result_path, argv[start_idx]);
        } else {
            // For relative paths, construct full path
            snprintf(result_path, PATH_MAX*2, "%s/%s", cwd, argv[start_idx]);
        }
        
        // Check if directory exists
        struct stat st;
        if (stat(result_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            return result_path;
        } else {
            return NULL; // Directory doesn't exist
        }
    }
}

// Check if we're outputting to a pipe or terminal
static int is_pipe_output() {
    return !isatty(STDOUT_FILENO);
} // is a POSIX system call that checks whether a file descriptor refers to a terminal (TTY) or not.

// List directory contents with pipeline awareness
static void list_directory(const char *dir_path, int show_hidden, int line_format) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "No such directory!\n");  // Error to stderr for pipeline compatibility
        return;
    }
    
    // Collect all entries
    char **entries = NULL;
    int count = 0;
    int capacity = 100;
    
    entries = malloc(capacity * sizeof(char *));
    if (!entries) {
        closedir(dir);
        return;
    }
    
    // Only add . and .. if show_hidden is true
    if (show_hidden) {
        if (count >= capacity) {
            capacity *= 2;
            char **new_entries = realloc(entries, capacity * sizeof(char *));
            if (!new_entries) {
                free(entries);
                closedir(dir);
                return;
            }
            entries = new_entries;
        }
        entries[count] = strdup(".");
        if (!entries[count]) {
            free(entries);
            closedir(dir);
            return;
        }
        count++;
        
        if (count >= capacity) {
            capacity *= 2;
            char **new_entries = realloc(entries, capacity * sizeof(char *));
            if (!new_entries) {
                for (int i = 0; i < count; i++) free(entries[i]);
                free(entries);
                closedir(dir);
                return;
            }
            entries = new_entries;
        }
        entries[count] = strdup("..");
        if (!entries[count]) {
            for (int i = 0; i < count; i++) free(entries[i]);
            free(entries);
            closedir(dir);
            return;
        }
        count++;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and .. since we handle them separately above
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Skip hidden files if not showing them
        if (!show_hidden && entry->d_name[0] == '.') {
            continue;
        }
        
        // Add entry to list
        if (count >= capacity) {
            capacity *= 2;
            char **new_entries = realloc(entries, capacity * sizeof(char *));
            if (!new_entries) {
                // Cleanup and exit
                for (int i = 0; i < count; i++) free(entries[i]);
                free(entries);
                closedir(dir);
                return;
            }
            entries = new_entries;
        }
        entries[count] = strdup(entry->d_name);
        if (!entries[count]) {
            for (int i = 0; i < count; i++) free(entries[i]);
            free(entries);
            closedir(dir);
            return;
        }
        count++;
    }
    closedir(dir);
    
    // Sort entries lexicographically using ASCII values
    if (count > 0) {
        qsort(entries, count, sizeof(char *), compare_strings);
    }
    
    // Determine output format based on flags and pipeline status
    int force_line_format = line_format || is_pipe_output();
    
    // Filter entries based on show_hidden flag and print
    if (force_line_format) {
        // Line by line format (-l flag set OR piped output)
        for (int i = 0; i < count; i++) {
            // Skip hidden files unless show_hidden is set
            if (!show_hidden && entries[i][0] == '.') {
                free(entries[i]);
                continue;
            }
            printf("%s\n", entries[i]);
            fflush(stdout);  // Ensure immediate output for pipelines
            free(entries[i]);
        }
    } else {
        // Default ls-like format (space-separated on one line) - only when outputting to terminal
        int printed_any = 0;
        for (int i = 0; i < count; i++) {
            // Skip hidden files unless show_hidden is set
            if (!show_hidden && entries[i][0] == '.') {
                free(entries[i]);
                continue;
            }
            if (printed_any) printf(" ");
            printf("%s", entries[i]);
            printed_any = 1;
            free(entries[i]);
        }
        if (printed_any) printf("\n");
        fflush(stdout);
    }
    free(entries);
}

// Pipeline-compatible glob expansion output
static void output_glob_matches(char **matches, int match_count, int line_format) {
    if (!matches || match_count == 0) {
        return;
    }
    
    // Force line format if outputting to pipe
    int force_line_format = line_format || is_pipe_output();
    // based on real ls behaviour or itll break pipelines -- assumption
    if (force_line_format) {
        for (int i = 0; i < match_count; i++) {
            printf("%s\n", matches[i]);
            fflush(stdout);  // Ensure immediate output for pipelines
        }
    } else {
        for (int i = 0; i < match_count; i++) {
            if (i > 0) printf(" ");
            printf("%s", matches[i]);
        }
        printf("\n");
        fflush(stdout);
    }
}

void reveal(int argc, char **argv) {
    int show_hidden = 0;
    int line_format = 0;
    int arg_idx = 1; // Start after command name
    char *target_dir = NULL;
    char *pattern = NULL;
    
    // Parse flags and find the first non-flag argument
    while (arg_idx < argc && argv[arg_idx][0] == '-') {
        char *flags = argv[arg_idx] + 1; // Skip the '-'
        
        // Handle empty flag (just '-')
        if (strlen(flags) == 0) {
            break; // '-' is a directory argument, not a flag
        }

        if (strchr(flags, '-') != NULL) {
    fprintf(stderr, "reveal: Invalid syntax!\n");
    return;
}
        
        // Process each character in the flag string
        for (int i = 0; flags[i] != '\0'; i++) {
            if (flags[i] == 'a') {
                show_hidden = 1;
            } else if (flags[i] == 'l') {
                line_format = 1;
            }
            // Ignore other flags (as per requirements)
        }
        arg_idx++;
    }
    
    // Check if we have a pattern argument
    if (arg_idx < argc) {
        pattern = argv[arg_idx];
        
        // Check for too many arguments
        int remaining_args = argc - arg_idx;
        if (remaining_args > 1) {
            fprintf(stderr, "reveal: Invalid Syntax!\n");  // Error to stderr
            return;
        }
        
        // If pattern contains glob characters, use current directory and apply pattern
        if (has_glob_chars(pattern)) {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) == NULL) {
                fprintf(stderr, "No such directory!\n");  // Error to stderr
                return;
            }
            target_dir = cwd;
            
            // Expand the glob pattern
            int match_count = 0;
            char **matches = expand_glob_pattern(target_dir, pattern, &match_count);
            
            if (matches && match_count > 0) {
                // Output matches
                output_glob_matches(matches, match_count, line_format);
                
                // Clean up
                for (int i = 0; i < match_count; i++) {
                    free(matches[i]);
                }
                free(matches);
            }
            // No matches found - this is not an error, just no output
            return;
        } else {
            // Not a glob pattern, treat as directory path
            target_dir = get_target_directory(argc, argv, arg_idx);
        }
    } else {
        // No arguments, use current directory
        target_dir = get_target_directory(argc, argv, arg_idx);
    }
    
    // Validate target directory
    if (!target_dir) {
        fprintf(stderr, "No such directory!\n");  // Error to stderr
        return;
    }
    
    // List directory contents normally
    list_directory(target_dir, show_hidden, line_format);
}