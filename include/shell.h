#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <limits.h>
#include <ctype.h>
#include <sys/utsname.h>
#include <fcntl.h>

#define MAX_INPUT_SIZE 1024
#define MAX_PATH_SIZE PATH_MAX

extern int hop_called;
// Function declarations
void display_prompt(void);
int get_user_input(char *input);
int parse_command(const char *input);
char* get_home_directory(void);
char* format_current_path(const char* home_dir);

// Parser function declarations
int is_valid_shell_cmd(const char *input);
int is_valid_cmd_group(const char *input);
int is_valid_atomic(const char *input);
int is_valid_name(const char *input);
int is_valid_input_redirect(const char *input);
int is_valid_output_redirect(const char *input);

// Utility functions
void trim_whitespace(char *str);
int skip_whitespace(const char *str, int start);

// Command execution
int execute_command(const char *input);

// History
void load_history(void);

// Input redirection functions
char* extract_input_redirect(const char *input, char **clean_command);
int setup_input_redirection(const char *filename);

void hop(int argc, char **argv);
void reveal(int argc, char **argv);
void log_command(int argc, char **argv);
void activities(int argc, char **argv);
void ping(int argc, char **argv);
void fg(int argc, char **argv);
void bg(int argc, char **argv);
void add_to_history(const char *command);

#endif