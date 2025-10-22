#include "shell.h"

int get_user_input(char *input) {
    if (!fgets(input, MAX_INPUT_SIZE, stdin)) {
        return -1; // EOF or error
    }
    
    // Remove newline character
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
        input[len - 1] = '\0';
    }
    
    return 0;
}