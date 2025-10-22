#include "shell.h"
#include "bg_jobs.h"
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

// Parse command line into argc and argv
static int parse_command_line(const char *input, char **argv, int max_args) {
    int argc = 0;
    int len = strlen(input);
    int i = 0;
    
    // Skip leading whitespace
    while (i < len && isspace(input[i])) i++;
    
    while (i < len && argc < max_args - 1) {
        // Find start of argument
        while (i < len && isspace(input[i])) i++;
        if (i >= len) break;
        
        int start = i;
        
        // Find end of argument
        while (i < len && !isspace(input[i])) i++;
        
        // Copy argument
        int arg_len = i - start;
        argv[argc] = malloc(arg_len + 1);
        strncpy(argv[argc], input + start, arg_len);
        argv[argc][arg_len] = '\0';
        argc++;
    }
    
    argv[argc] = NULL; // Null terminate
    return argc;
}
// make a list of all arguments and store like ls -l acbd will be stores as [ls, l, abcd, NULL]
// Free allocated argv memory
static void free_argv(char **argv, int argc) {
    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
}

// Local trim function that returns a newly allocated trimmed copy
static char* str_trim_new(const char *s) {
    if (!s) return NULL;
    const char *start = s;
    while (*start && isspace(*start)) start++;
    const char *end = s + strlen(s);
    if (end > start) {
        end--;
        while (end > start && isspace(*end)) end--;
        end++;
    }
    size_t n = (size_t)(end - start);
    char *out = malloc(n + 1);
    memcpy(out, start, n);
    out[n] = '\0';
    return out;
} // removes trailing and leading whitespace 

// Check if command contains 'log' anywhere in the pipeline
static int contains_log_command(const char *input) {
    if (!input) return 0;
    
    // Create a copy to tokenize
    char *input_copy = strdup(input);
   // char *token;
    // char *saveptr;
    
    // Split by various separators to check each command part
    char *current = input_copy;
    while (current && *current) {
        // Find next separator (|, ;, &)
        char *next_sep = NULL;
        char *pipe_pos = strchr(current, '|');
        char *semi_pos = strchr(current, ';');
        char *amp_pos = strchr(current, '&');
        
        // Find the earliest separator
        next_sep = pipe_pos;
        if (semi_pos && (!next_sep || semi_pos < next_sep)) next_sep = semi_pos;
        if (amp_pos && (!next_sep || amp_pos < next_sep)) next_sep = amp_pos;
        
        // Extract current command segment
        char *segment;
        if (next_sep) {
            *next_sep = '\0';
            segment = str_trim_new(current);
            current = next_sep + 1;
        } else {
            segment = str_trim_new(current);
            current = NULL;
        }
        
        // Check if this segment starts with 'log'
        if (segment && strlen(segment) >= 3) {
            if (strncmp(segment, "log", 3) == 0 && 
                (segment[3] == '\0' || isspace(segment[3]))) {
                free(segment);
                free(input_copy);
                return 1;
            }
        }
        
        free(segment);
    }
    
    free(input_copy);
    return 0;
} // check if log is there ANYWHERE in pipeline 

// Extract both input and output redirections from a single command string.
// Returns clean_command (no redirection tokens), last input filename, last output filename, and append flag.
// FAILS FAST on first redirection error like bash does
static void extract_redirections(const char *input,
    char **clean_command,
    char **in_filename,
    char **out_filename,
    int *out_append,
    int *error_occurred)  // New parameter to indicate if any error occurred
{
    *clean_command = NULL; // command without redirections
    *in_filename = NULL; // input filename
    *out_filename = NULL; // output filename
    *error_occurred = 0; // indicates error
    if (out_append) *out_append = 0; // >> what file to append to
    if (!input) return;

    size_t n = strlen(input);
    char *clean = malloc(n + 1);
    int clean_len = 0;
    char *last_in = NULL;
    char *last_out = NULL;
    int last_out_app = 0;
    // track the most recent input/output filenames

    int i = 0;
    while (i < (int)n && !*error_occurred) {  // STOP on first error
        while (i < (int)n && isspace(input[i])) i++;
        if (i >= (int)n) break;
        int start = i;
        while (i < (int)n && !isspace(input[i])) i++;
        int tok_len = i - start;
        if (tok_len == 0) continue; // loops over tokens separated by whitespace 

        // Check for standalone redirection operators
        if (tok_len == 1 && input[start] == '<') {
            // read filename
            while (i < (int)n && isspace(input[i])) i++;
            int fstart = i;
            while (i < (int)n && !isspace(input[i])) i++;
            int flen = i - fstart;
            if (flen > 0) {
                char *fname = strndup(input + fstart, flen);
                // Try to open the file to check for errors - FAIL IMMEDIATELY
                int fd = open(fname, O_RDONLY);
                if (fd == -1) {
                    fprintf(stderr, "No such file or directory\n");
                    *error_occurred = 1;
                    free(fname);
                    // Clean up any previously allocated filenames
                    if (last_in) free(last_in);
                    if (last_out) free(last_out);
                    last_in = NULL;
                    last_out = NULL;
                    break; // STOP PROCESSING
                } else {
                    close(fd);
                    if (last_in) free(last_in);  // Free previous input file
                    last_in = fname;
                }
            }
            continue;
        }
        
        if (input[start] == '>' && tok_len >= 1) {
            int is_append = 0;
            int redirect_len = 1;
            
            if (tok_len >= 2 && input[start] == '>' && input[start + 1] == '>') {
                is_append = 1;
                redirect_len = 2;
            }
            
            char *fname = NULL;
            // Check if it's a standalone operator
            if (tok_len == redirect_len) {
                while (i < (int)n && isspace(input[i])) i++;
                int fstart = i;
                while (i < (int)n && !isspace(input[i])) i++;
                int flen = i - fstart;
                if (flen > 0) {
                    fname = strndup(input + fstart, flen);
                }
            } 
            // Check if it's attached redirection
            else if (tok_len > redirect_len) {
                int fname_start = start + redirect_len;
                int fname_len = tok_len - redirect_len;
                if (fname_len > 0) {
                    fname = strndup(input + fname_start, fname_len);
                }
            }

            if (fname) {
                // Try to open the file to check for errors - FAIL IMMEDIATELY
                int flags = O_WRONLY | O_CREAT;
                flags |= (is_append ? O_APPEND : O_TRUNC);
                int fd = open(fname, flags, 0644);
                if (fd == -1) {
                    fprintf(stderr, "Unable to create file for writing");
                    *error_occurred = 1;
                    free(fname);
                    // Clean up any previously allocated filenames
                    if (last_in) free(last_in);
                    if (last_out) free(last_out);
                    last_in = NULL;
                    last_out = NULL;
                    break; // STOP PROCESSING
                } else {
                    close(fd);
                    if (last_out) free(last_out);  // Free previous output file
                    last_out = fname;
                    last_out_app = is_append;
                }
            }
            continue;
        }
        
        // Check for attached input redirection
        if (input[start] == '<' && tok_len > 1) {
            int fname_start = start + 1;
            int fname_len = tok_len - 1;
            if (fname_len > 0) {
                char *fname = strndup(input + fname_start, fname_len);
                // Try to open the file to check for errors - FAIL IMMEDIATELY
                int fd = open(fname, O_RDONLY);
                if (fd == -1) {
                    fprintf(stderr, "bash: %s: %s\n", fname, strerror(errno));
                    *error_occurred = 1;
                    free(fname);
                    // Clean up any previously allocated filenames
                    if (last_in) free(last_in);
                    if (last_out) free(last_out);
                    last_in = NULL;
                    last_out = NULL;
                    break; // STOP PROCESSING
                } else {
                    close(fd);
                    if (last_in) free(last_in);  // Free previous input file
                    last_in = fname;
                }
            }
            continue;
        }

        // Normal argument - append to clean command
        if (clean_len > 0) clean[clean_len++] = ' ';
        memcpy(clean + clean_len, input + start, tok_len);
        clean_len += tok_len;
    }

    clean[clean_len] = '\0';
    *clean_command = str_trim_new(clean);
    free(clean);
    
    // Only set output parameters if no error occurred
    if (*error_occurred) {
        *in_filename = NULL;
        *out_filename = NULL;
        if (out_append) *out_append = 0;
    } else {
        *in_filename = last_in;
        *out_filename = last_out;
        if (out_append) *out_append = last_out_app;
    }
}
// ============================ LLM GENERATED CODE BEGINS ==============================================
// Setup input redirection for a command - FIXED error handling
int setup_input_redirection(const char *filename) {
    if (!filename || strlen(filename) == 0) {
        return 0;
    }
    
    // Open the file for reading
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "No such file or directory\n");
        return -1;
    }
    
    // Redirect stdin to the file
    if (dup2(fd, STDIN_FILENO) == -1) {
        perror("dup2");
        close(fd);
        return -1;
    }
    
    // Close the original file descriptor
    close(fd);
    
    return 0;
}

// Setup output redirection for a command - FIXED error handling
static int setup_output_redirection(const char *filename, int append) {
    if (!filename || strlen(filename) == 0) {
        return 0;
    }
    int flags = append ? (O_WRONLY | O_CREAT | O_APPEND) : (O_WRONLY | O_CREAT | O_TRUNC);
    int fd = open(filename, flags, 0644);
    if (fd == -1) {
        fprintf(stderr, "Unable to create file for writing\n");
        return -1;
    }
    if (dup2(fd, STDOUT_FILENO) == -1) {
        perror("dup2"); // Makes the file descriptor fd replace the standard output.

//From now on, the command will write to the file instead of the terminal.
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}
    // ======================================= LLM GENERATED CODE ENDS ==================================================
// Split a command line by pipes into an array of strings. Returns count. Allocates results.
static int split_pipeline(const char *input, char ***out_cmds) {
    int count = 1;
    for (const char *p = input; *p; ++p) if (*p == '|') count++; // splits at pipw
    char **parts = calloc((size_t)count, sizeof(char*));
    int idx = 0;
    const char *seg_start = input;
    for (const char *p = input; ; ++p) {
        if (*p == '|' || *p == '\0') {
            char *segment = strndup(seg_start, (size_t)(p - seg_start));
            char *trimmed = str_trim_new(segment);
            free(segment);
            parts[idx++] = trimmed;
            if (*p == '\0') break;
            seg_start = p + 1;
        }
    }
    *out_cmds = parts;
    return idx;
} // splits command line into pipe segments 

// Free array from split_pipeline
static void free_pipeline(char **cmds, int count) {
    for (int i = 0; i < count; ++i) free(cmds[i]);
    free(cmds);
}

// ============================== LLM GENERATED CODE BEGINS ==============================================
static int split_sequential_with_bg(const char *input, char ***out_cmds, int **out_bg_flags) {
    int count = 0; // splits based on ; sequential operators (allows one after the other execution) and respects & bg operators 
    int capacity = 10;
    char **parts = malloc(capacity * sizeof(char*));
    int *bg_flags = malloc(capacity * sizeof(int));
    
    const char *start = input;
    int in_quote = 0;
    char quote_char = 0;
    
    for (const char *p = input; ; ++p) {
        if (*p == '\0' || (!in_quote && *p == ';')) {
            // Extract segment up to semicolon or end
            size_t len = p - start;
            char *segment = malloc(len + 1);
            strncpy(segment, start, len);
            segment[len] = '\0';
            
            // Now process this segment for & operators
            const char *seg_start = segment;
            const char *seg_p = segment;
            
            while (*seg_p) {
                if (*seg_p == '&' && !in_quote) {
                    // Found & - extract command before it
                    size_t cmd_len = seg_p - seg_start;
                    if (cmd_len > 0) {
                        char *cmd = malloc(cmd_len + 1);
                        strncpy(cmd, seg_start, cmd_len);
                        cmd[cmd_len] = '\0';
                        
                        char *trimmed = str_trim_new(cmd);
                        free(cmd);
                        
                        if (strlen(trimmed) > 0) {
                            if (count >= capacity) {
                                capacity *= 2;
                                parts = realloc(parts, capacity * sizeof(char*));
                                bg_flags = realloc(bg_flags, capacity * sizeof(int));
                            }
                            parts[count] = trimmed;
                            bg_flags[count] = 1; // This command is background
                            count++;
                        } else {
                            free(trimmed);
                        }
                    }
                    
                    // Move past the &
                    seg_p++;
                    while (*seg_p && isspace(*seg_p)) seg_p++;
                    seg_start = seg_p;
                    continue;
                }
                
                if (*seg_p == '\'' || *seg_p == '"') {
                    if (!in_quote) {
                        in_quote = 1;
                        quote_char = *seg_p;
                    } else if (*seg_p == quote_char) {
                        in_quote = 0;
                    }
                }
                seg_p++;
            }
            
            // Handle remaining part after last & (or entire segment if no &)
            if (seg_start < seg_p) {
                size_t remaining_len = seg_p - seg_start;
                char *remaining = malloc(remaining_len + 1);
                strncpy(remaining, seg_start, remaining_len);
                remaining[remaining_len] = '\0';
                // ignores semicolons inside quotes
                char *trimmed = str_trim_new(remaining);
                free(remaining);
                // takes segments separated by ;
                if (strlen(trimmed) > 0) {
                    if (count >= capacity) {
                        capacity *= 2;
                        parts = realloc(parts, capacity * sizeof(char*));
                        bg_flags = realloc(bg_flags, capacity * sizeof(int));
                    }
                    parts[count] = trimmed;
                    bg_flags[count] = 0; // This command is foreground
                    count++;
                } else {
                    free(trimmed);
                }
            }
            // ======================= LLM GENERATED CODE ENDS ===========================================
            
            free(segment);
            if (*p == '\0') break;
            start = p + 1;
        }
        else if (*p == '\'' || *p == '"') {
            if (!in_quote) {
                in_quote = 1;
                quote_char = *p;
            } else if (*p == quote_char) {
                in_quote = 0;
            }
        }
    }
    
    *out_cmds = parts;
    *out_bg_flags = bg_flags;
    return count;
}

// Free array from split_sequential_with_bg
static void free_sequential_with_bg(char **cmds, int *bg_flags, int count) {
    for (int i = 0; i < count; ++i) free(cmds[i]);
    free(cmds);
    free(bg_flags);
}

// Execute a single command string (no pipes), supporting <, >, >>
// FIXED: Proper redirection order - file redirections only apply if they don't conflict with pipes
static void exec_single_command(const char *cmdline) {
    // executes a single command string , handles I/O, builtins, externals, MEANT TO BE CALLED INSIDE CHILD PROCESS BY FORK()

    char *clean = NULL;
    char *in_file = NULL;
    char *out_file = NULL;
    int out_append = 0;
    int error_occurred = 0;
    
    extract_redirections(cmdline, &clean, &in_file, &out_file, &out_append, &error_occurred);
    
    if (error_occurred) {
        free(clean);
        free(in_file);
        free(out_file);
        exit(EXIT_FAILURE);
    }

    char *argv[64];
    int argc = parse_command_line(clean ? clean : "", argv, 64);
    if (argc == 0) {
        free(clean);
        free(in_file);
        free(out_file);
        exit(EXIT_SUCCESS);
    }

    // Check if stdin/stdout are already redirected (from pipes)
    int stdin_is_pipe = !isatty(STDIN_FILENO);
    int stdout_is_pipe = !isatty(STDOUT_FILENO);
    
    // Apply input redirection ONLY if stdin is not already redirected from a pipe
    // AND if we have an explicit input file
    if (in_file && !stdin_is_pipe) {
        if (setup_input_redirection(in_file) == -1) {
            free_argv(argv, argc);
            free(clean); free(in_file); free(out_file);
            exit(EXIT_FAILURE);
        }
    }
    
    // Apply output redirection ONLY if stdout is not already redirected to a pipe
    // AND if we have an explicit output file
    if (out_file && !stdout_is_pipe) {
        if (setup_output_redirection(out_file, out_append) == -1) {
            free_argv(argv, argc);
            free(clean); free(in_file); free(out_file);
            exit(EXIT_FAILURE);
        }
    }

    // Built-ins in child (simple route)
    if (strcmp(argv[0], "hop") == 0) {
        hop(argc, argv);
        free_argv(argv, argc);
        free(clean); free(in_file); free(out_file);
        exit(EXIT_SUCCESS);
    } else if (strcmp(argv[0], "reveal") == 0) {
        reveal(argc, argv);
        free_argv(argv, argc);
        free(clean); free(in_file); free(out_file);
        exit(EXIT_SUCCESS);
    } else if (strcmp(argv[0], "log") == 0) {
        log_command(argc, argv);
        free_argv(argv, argc);
        free(clean); free(in_file); free(out_file);
        exit(EXIT_SUCCESS);
    }

    // External command
    if (execvp(argv[0], argv) == -1) {
        fprintf(stderr, "Command not found!\n");
        free_argv(argv, argc);
        free(clean); free(in_file); free(out_file);
        _exit(127);
    }
}

// Check if a command is a built-in
static int is_builtin(const char *cmd) {
    return (strcmp(cmd, "hop") == 0 || 
            strcmp(cmd, "reveal") == 0 || 
            strcmp(cmd, "log") == 0 ||
            strcmp(cmd, "activities") == 0 ||
            strcmp(cmd, "ping") == 0 ||
            strcmp(cmd, "fg") == 0 ||
            strcmp(cmd, "bg") == 0);
} 
// Execute a built-in command in the current process with redirection support
static void execute_builtin(const char *cmdline) {
    char *clean = NULL;
    char *in_file = NULL;
    char *out_file = NULL;
    int out_append = 0;
    int error_occurred = 0;
    
    extract_redirections(cmdline, &clean, &in_file, &out_file, &out_append, &error_occurred);
    
    if (error_occurred) {
        free(clean);
        free(in_file);
        free(out_file);
        return;
    }

    char *argv[64];
    int argc = parse_command_line(clean ? clean : "", argv, 64);
    if (argc == 0) {
        free(clean);
        free(in_file);
        free(out_file);
        return;
    }

    // Save original stdin/stdout for restoration
    int saved_stdin = -1, saved_stdout = -1;
    
    // Setup input redirection if needed
    if (in_file) {
        saved_stdin = dup(STDIN_FILENO);
        if (saved_stdin == -1 || setup_input_redirection(in_file) == -1) {
            if (saved_stdin != -1) close(saved_stdin);
            free_argv(argv, argc);
            free(clean); free(in_file); free(out_file);
            return;
        }
    }
    
    // Setup output redirection if needed
    if (out_file) {
        saved_stdout = dup(STDOUT_FILENO);
        if (saved_stdout == -1 || setup_output_redirection(out_file, out_append) == -1) {
            if (saved_stdout != -1) close(saved_stdout);
            if (saved_stdin != -1) {
                dup2(saved_stdin, STDIN_FILENO);
                close(saved_stdin);
            }
            free_argv(argv, argc);
            free(clean); free(in_file); free(out_file);
            return;
        }
    }

    // Execute the built-in command
    if (strcmp(argv[0], "hop") == 0) {
        hop(argc, argv);
    } else if (strcmp(argv[0], "reveal") == 0) {
        reveal(argc, argv);
    } else if (strcmp(argv[0], "log") == 0) {
        log_command(argc, argv);
    } else if (strcmp(argv[0], "activities") == 0) {
        activities(argc, argv);
    } else if (strcmp(argv[0], "ping") == 0) {
        ping(argc, argv);
    } else if (strcmp(argv[0], "fg") == 0) {
        fg(argc, argv);
    } else if (strcmp(argv[0], "bg") == 0) {
        bg(argc, argv);
    }
    
    // Restore original stdin/stdout
    if (saved_stdin != -1) {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
    }
    if (saved_stdout != -1) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }

    free_argv(argv, argc);
    free(clean); free(in_file); free(out_file);
    fflush(stdout);
    fflush(stderr);
}
// ======================== LLM GENERATED CODE BEGINS =======================================
// Execute a single complete shell command (may contain pipes, but no semicolons)
static void execute_single_shell_command(const char *input, int is_background) {
    if (!input || strlen(input) == 0) {
        return;
    }

    // Check if this is a simple built-in command (no pipes)
    if (strchr(input, '|') == NULL) {
        // No pipes - check if it's a built-in
        char *clean = NULL;
        char *in_file = NULL;
        char *out_file = NULL;
        int out_append = 0;
        int error_occurred = 0;
        
        // Extract redirections and check for errors
        extract_redirections(input, &clean, &in_file, &out_file, &out_append, &error_occurred);
        
        if (error_occurred) {
            free(clean);
            free(in_file);
            free(out_file);
            return;
        }
        
        char *argv[64];
        int argc = parse_command_line(clean ? clean : "", argv, 64);
        
        if (argc > 0 && is_builtin(argv[0])) {
            if (is_background) {
                // For background built-ins, fork and execute in child process
                pid_t pid = fork();
                if (pid == -1) {
                    perror("fork");
                } else if (pid == 0) {
                    // Child process - execute built-in
                    // Set new process group for background job
                    setpgid(0, 0);
                    // Redirect stdin from /dev/null for background processes
                    freopen("/dev/null", "r", stdin);
                    
                    execute_builtin(input);
                    exit(EXIT_SUCCESS);
                } else {
                    // Parent process - track background job
                    setpgid(pid, pid);
                    char *cmd_copy = strdup(input);
                    char *first_token = strtok(cmd_copy, " \t\n");
                    if (first_token) {
                        char *cmd_name = strrchr(first_token, '/');
                        cmd_name = cmd_name ? cmd_name + 1 : first_token;
                        add_background_job(pid, cmd_name);
                    }
                    free(cmd_copy);
                }
            } else {
                // Execute built-in in current process
                execute_builtin(input);
            }
            free_argv(argv, argc);
            free(clean); free(in_file); free(out_file);
            return;
        }
        
        // Clean up temporary parsing
        free_argv(argv, argc);
        free(clean); free(in_file); free(out_file);
        
        // Not a built-in, fork and execute as external command
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            return;
        } else if (pid == 0) {
            // Child process
            
            // Create new process group for job control
            setpgid(0, 0);
            
            if (is_background) {
                // Redirect stdin from /dev/null for background processes
                freopen("/dev/null", "r", stdin);
            } else {
                // Give terminal control to foreground process group
                tcsetpgrp(STDIN_FILENO, getpid());
            }
            
            exec_single_command(input);
            // never returns
        } else {
            // Parent process
            
            // Set process group for the child
            setpgid(pid, pid);
            
            if (is_background) {
                // Extract command name for job tracking
                char *cmd_copy = strdup(input);
                char *first_token = strtok(cmd_copy, " \t\n");
                if (first_token) {
                    // Get just the command name without path
                    char *cmd_name = strrchr(first_token, '/');
                    cmd_name = cmd_name ? cmd_name + 1 : first_token;
                    add_background_job(pid, cmd_name);
                }
                free(cmd_copy);
            } else {
                // Set as foreground process for signal handling
                set_foreground_process(pid, pid);
                
                // Wait for foreground process
                int status;
                pid_t result = waitpid(pid, &status, WUNTRACED);
                
                if (result > 0 && WIFSTOPPED(status)) {
                    // Process was stopped (Ctrl-Z), move to background
                    char *cmd_copy = strdup(input);
                    char *first_token = strtok(cmd_copy, " \t\n");
                    if (first_token) {
                        char *cmd_name = strrchr(first_token, '/');
                        cmd_name = cmd_name ? cmd_name + 1 : first_token;
                        int job_id = add_stopped_job(pid, cmd_name);
                        printf("[%d] Stopped %s\n", job_id, cmd_name);
                    }
                    free(cmd_copy);
                }
                
                // Clear foreground process
                clear_foreground_process();
                
                // Give terminal control back to shell
                tcsetpgrp(STDIN_FILENO, getpgrp());
            }
        }
    } else {
        // Pipeline - validate redirections in each command first
        char **cmds = NULL;
        int ncmds = split_pipeline(input, &cmds);
        if (ncmds <= 0) {
            return;
        }
        
        // Pre-validate ALL redirections in pipeline
        int pipeline_has_errors = 0;
        for (int i = 0; i < ncmds; i++) {
            char *clean = NULL;
            char *in_file = NULL;
            char *out_file = NULL;
            int out_append = 0;
            int error_occurred = 0;
            
            extract_redirections(cmds[i], &clean, &in_file, &out_file, &out_append, &error_occurred);
            
            if (error_occurred) {
                pipeline_has_errors = 1;
            }
            
            free(clean); 
            free(in_file); 
            free(out_file);
        }
        
        if (pipeline_has_errors) {
            free_pipeline(cmds, ncmds);
            return;
        }
        
        pid_t pipeline_pgid = 0;
        
        if (is_background) {
            // For background pipelines, fork once and run entire pipeline in background
            pid_t main_pid = fork();
            if (main_pid == -1) {
                perror("fork");
                free_pipeline(cmds, ncmds);
                return;
            } else if (main_pid == 0) {
                // Child: create new process group and redirect stdin from /dev/null
                setpgid(0, 0);
                freopen("/dev/null", "r", stdin);
                // Continue with pipeline execution below
                pipeline_pgid = getpid();
            } else {
                // Parent: add to background jobs and return
                setpgid(main_pid, main_pid);
                char *cmd_copy = strdup(input);
                char *first_token = strtok(cmd_copy, " \t\n|");
                if (first_token) {
                    char *cmd_name = strrchr(first_token, '/');
                    cmd_name = cmd_name ? cmd_name + 1 : first_token;
                    add_background_job(main_pid, cmd_name);
                }
                free(cmd_copy);
                free_pipeline(cmds, ncmds);
                return;
            }
        }
        
        int pipes_needed = ncmds - 1;
        
        // Dynamically allocate pipe file descriptors
        int (*pipefd)[2] = malloc(pipes_needed * sizeof(int[2]));
        if (!pipefd) {
            perror("malloc");
            free_pipeline(cmds, ncmds);
            if (is_background) exit(EXIT_FAILURE);
            return;
        }
        
        // Create all pipes
        for (int i = 0; i < pipes_needed; ++i) {
            if (pipe(pipefd[i]) == -1) {
                perror("pipe");
                // Close any pipes we've already created
                for (int j = 0; j < i; j++) {
                    close(pipefd[j][0]);
                    close(pipefd[j][1]);
                }
                free(pipefd);
                free_pipeline(cmds, ncmds);
                if (is_background) exit(EXIT_FAILURE);
                return;
            }
        }

        // Dynamically allocate process IDs array
        pid_t *pids = malloc(ncmds * sizeof(pid_t));
        if (!pids) {
            perror("malloc");
            for (int i = 0; i < pipes_needed; i++) {
                close(pipefd[i][0]);
                close(pipefd[i][1]);
            }
            free(pipefd);
            free_pipeline(cmds, ncmds);
            if (is_background) exit(EXIT_FAILURE);
            return;
        }

        // Fork and execute each command in the pipeline
        for (int i = 0; i < ncmds; ++i) {
            pids[i] = fork();
            if (pids[i] == -1) {
                perror("fork");
                pids[i] = 0; // Mark as invalid for cleanup
                continue;
            }
            if (pids[i] == 0) {
                // child i
                
                // Set process group (all processes in pipeline share same pgid)
                if (i == 0) {
                    setpgid(0, 0);
                    pipeline_pgid = getpid();
                } else {
                    setpgid(0, pipeline_pgid);
                }
                
                // Give terminal control to foreground pipeline (if not background)
                if (!is_background && i == 0) {
                    tcsetpgrp(STDIN_FILENO, pipeline_pgid);
                }
                
                // Connect input from previous pipe unless overridden later by explicit '<'
                if (i > 0) {
                    if (dup2(pipefd[i-1][0], STDIN_FILENO) == -1) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                }
                // Connect output to next pipe unless overridden later by explicit '>'
                if (i < ncmds - 1) {
                    if (dup2(pipefd[i][1], STDOUT_FILENO) == -1) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                }
                // Close all pipe fds in child before applying file redirections
                for (int k = 0; k < pipes_needed; ++k) {
                    close(pipefd[k][0]);
                    close(pipefd[k][1]);
                }
                // Now parse and apply per-command redirections which override pipes if present
                exec_single_command(cmds[i]);
                // If exec_single_command were to return, treat as failure
                fprintf(stderr, "Command not found!\n");
                _exit(127);
            } else {
                // Parent: set process group for each child
                if (i == 0) {
                    pipeline_pgid = pids[i];
                    setpgid(pids[i], pids[i]);
                } else {
                    setpgid(pids[i], pipeline_pgid);
                }
            }
        }
        
        // Parent (or background process parent): close all pipe fds
        for (int k = 0; k < pipes_needed; ++k) {
            close(pipefd[k][0]);
            close(pipefd[k][1]);
        }
        
        if (!is_background) {
            // Set pipeline as foreground for signal handling
            set_foreground_process(pids[0], pipeline_pgid);
        }
        
        // Wait for pipeline processes correctly
        if (!is_background) {
            int pipeline_stopped = 0;
            int processes_remaining = ncmds;
            
            while (processes_remaining > 0 && !pipeline_stopped) {
                int status;
                pid_t result = waitpid(-pipeline_pgid, &status, WUNTRACED);
                
                if (result > 0) {
                    if (WIFSTOPPED(status)) {
                        // Entire pipeline was stopped
                        pipeline_stopped = 1;
                        char *cmd_copy = strdup(input);
                        char *first_token = strtok(cmd_copy, " \t\n|");
                        if (first_token) {
                            char *cmd_name = strrchr(first_token, '/');
                            cmd_name = cmd_name ? cmd_name + 1 : first_token;
                            int job_id = add_stopped_job(pipeline_pgid, cmd_name);
                            printf("[%d] Stopped %s\n", job_id, cmd_name);
                        }
                        free(cmd_copy);
                        break;
                    } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                        // A process in the pipeline finished
                        processes_remaining--;
                    }
                } else if (result == -1 && errno == ECHILD) {
                    // No more children to wait for
                    break;
                }
            }
        } else {
            // For background processes, don't wait
            // They'll be reaped by check_background_processes()
        }
        
        // Clear foreground process and give terminal back to shell
        if (!is_background) {
            clear_foreground_process();
            tcsetpgrp(STDIN_FILENO, getpgrp());
        }
        
        // Cleanup
        free(pipefd);
        free(pids);
        free_pipeline(cmds, ncmds);
        
        // If this was a background pipeline, exit the background process
        if (is_background) {
            exit(EXIT_SUCCESS);
        }
    }
}

// ==================================== LLM GENERATED CODE ENDS ===================================================

int execute_command(const char *input) {
    if (!input || strlen(input) == 0) {
        return 0;
    }

    // Check if command contains 'log' anywhere - if so, don't add to history
    int should_add_to_history = !contains_log_command(input);

    // Check for command separators (semicolons or ampersands)
    if (strchr(input, ';') != NULL || strchr(input, '&') != NULL) {
        // Split by separators and handle background flags per command
        char **seq_cmds = NULL;
        int *bg_flags = NULL;
        int nseq = split_sequential_with_bg(input, &seq_cmds, &bg_flags);
        
        for (int i = 0; i < nseq; i++) {
            if (seq_cmds[i] && strlen(seq_cmds[i]) > 0) {
                execute_single_shell_command(seq_cmds[i], bg_flags[i]);
            }
        }
        
        free_sequential_with_bg(seq_cmds, bg_flags, nseq);
    } else {
        // Single command with no separators (may contain pipes)
        execute_single_shell_command(input, 0);
    }

    // Add entire command line to history only if it doesn't contain 'log'
    if (should_add_to_history) {
        add_to_history(input);
    }
    
    return 1;
}