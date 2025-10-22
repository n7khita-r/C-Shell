#include "shell.h"

// Parser state structure
typedef struct {
    const char *input;
    int pos;
    int len;
} parser_state_t;

// recursive descent parser to validate syntax

// Helper functions for parsing
static int peek_char(parser_state_t *state); // look at char don't move
static int consume_char(parser_state_t *state); // read a char and move forward
static void skip_ws(parser_state_t *state); // skip spaces and tabs
static int parse_name(parser_state_t *state);
static int parse_input_redirect(parser_state_t *state);
static int parse_output_redirect(parser_state_t *state);
static int parse_atomic(parser_state_t *state);
static int parse_cmd_group(parser_state_t *state);
static int parse_shell_cmd(parser_state_t *state);

int parse_command(const char *input) {
    return is_valid_shell_cmd(input);
}

int is_valid_shell_cmd(const char *input) {
    if (!input || strlen(input) == 0) {
        return 0;
    }
    
    // Create parser state
    parser_state_t state;
    state.input = input;
    state.pos = 0;
    state.len = strlen(input);
    
    // Skip initial whitespace
    skip_ws(&state);
    
    // Empty input after trimming
    if (state.pos >= state.len) {
        return 0;
    }
    
    // Parse the shell command
    int result = parse_shell_cmd(&state);
    
    // Should consume entire input
    skip_ws(&state);
    return result && (state.pos >= state.len);
}

// Parse shell_cmd -> cmd_group ((& | ;) cmd_group)* &?
static int parse_shell_cmd(parser_state_t *state) {
    // Parse first cmd_group (required)
    if (!parse_cmd_group(state)) {
        return 0;
    }
    
    // Parse optional ((& | ;) cmd_group)*
    while (1) {
        skip_ws(state);
        
        // Check for & or ;
        if (peek_char(state) == '&' || peek_char(state) == ';') {
            char separator = consume_char(state);
            skip_ws(state);
            
            // If we just have & at the end, that's valid (background)
            if (separator == '&' && state->pos >= state->len) {
                break;
            }
            
            // Otherwise we need another cmd_group
            if (!parse_cmd_group(state)) {
                return 0;
            }
        } else {
            break;
        }
    }
    
    // Check for optional trailing &
    skip_ws(state);
    if (peek_char(state) == '&') {
        consume_char(state);
    }
    
    return 1;
}

// Parse cmd_group -> atomic (| atomic)*
static int parse_cmd_group(parser_state_t *state) {
    // Parse first atomic (required)
    if (!parse_atomic(state)) {
        return 0;
    }
    
    // Parse optional (| atomic)*
    while (1) {
        skip_ws(state);
        
        if (peek_char(state) == '|') {
            consume_char(state);
            skip_ws(state);
            
            // Must have another atomic after |
            if (!parse_atomic(state)) {
                return 0;
            }
        } else {
            break;
        }
    }
    
    return 1;
}

// Parse atomic -> name (name | input | output)*
static int parse_atomic(parser_state_t *state) {
    // Parse command name (required)
    if (!parse_name(state)) {
        return 0;
    }
    
    // Parse optional (name | input | output)*
    while (1) {
        int saved_pos = state->pos;
        skip_ws(state);
        
        // Try to parse input redirection
        if (peek_char(state) == '<') {
            if (parse_input_redirect(state)) {
                continue;
            }
            state->pos = saved_pos;
        }
        
        // Try to parse output redirection
        if (peek_char(state) == '>') {
            if (parse_output_redirect(state)) {
                continue;
            }
            state->pos = saved_pos;
        }
        
        // Try to parse another name (argument)
        if (parse_name(state)) {
            continue;
        }
        
        // Nothing more to parse
        state->pos = saved_pos;
        break;
    }
    
    return 1;
}

// Parse name -> [^|&><;]+
static int parse_name(parser_state_t *state) {
    skip_ws(state);
    int start_pos = state->pos;
    
    // Parse characters that are not special operators
    while (state->pos < state->len) {
        char c = peek_char(state);
        if (c == '|' || c == '&' || c == '>' || c == '<' || c == ';' || isspace(c)) {
            break;
        }
        consume_char(state);
    }
    
    // Must have consumed at least one character
    return state->pos > start_pos;
}

// Parse input -> < name
static int parse_input_redirect(parser_state_t *state) {
    if (peek_char(state) != '<') {
        return 0;
    }
    
    consume_char(state); // consume '<'
    skip_ws(state);
    
    // Must have a filename after <
    return parse_name(state);
}

// Parse output -> (> | >>) name  
static int parse_output_redirect(parser_state_t *state) {
    if (peek_char(state) != '>') {
        return 0;
    }
    
    consume_char(state); // consume first '>'
    
    // Check for >> (append)
    if (peek_char(state) == '>') {
        consume_char(state); // consume second '>'
    }
    
    skip_ws(state);
    
    // Must have a filename after > or >>
    return parse_name(state);
}

// Helper function implementations
static int peek_char(parser_state_t *state) {
    if (state->pos >= state->len) {
        return '\0';
    }
    return state->input[state->pos];
}

static int consume_char(parser_state_t *state) {
    if (state->pos >= state->len) {
        return '\0';
    }
    return state->input[state->pos++];
}

static void skip_ws(parser_state_t *state) {
    while (state->pos < state->len && isspace(state->input[state->pos])) {
        state->pos++;
    }
}

// Legacy functions (kept for compatibility)
int is_valid_cmd_group(const char *input) {
    parser_state_t state;
    state.input = input;
    state.pos = 0;
    state.len = strlen(input);
    skip_ws(&state);
    
    int result = parse_cmd_group(&state);
    skip_ws(&state);
    return result && (state.pos >= state.len);
}

int is_valid_atomic(const char *input) {
    parser_state_t state;
    state.input = input;
    state.pos = 0;
    state.len = strlen(input);
    skip_ws(&state);
    
    int result = parse_atomic(&state);
    skip_ws(&state);
    return result && (state.pos >= state.len);
}

int is_valid_name(const char *input) {
    if (!input || strlen(input) == 0) {
        return 0;
    }
    
    parser_state_t state;
    state.input = input;
    state.pos = 0;
    state.len = strlen(input);
    skip_ws(&state);
    
    int result = parse_name(&state);
    skip_ws(&state);
    return result && (state.pos >= state.len);
}

int is_valid_input_redirect(const char *input) {
    if (!input || strlen(input) == 0) {
        return 0;
    }
    
    parser_state_t state;
    state.input = input;
    state.pos = 0;
    state.len = strlen(input);
    skip_ws(&state);
    
    int result = parse_input_redirect(&state);
    skip_ws(&state);
    return result && (state.pos >= state.len);
}

int is_valid_output_redirect(const char *input) {
    if (!input || strlen(input) == 0) {
        return 0;
    }
    
    parser_state_t state;
    state.input = input;
    state.pos = 0;
    state.len = strlen(input);
    skip_ws(&state);
    
    int result = parse_output_redirect(&state);
    skip_ws(&state);
    return result && (state.pos >= state.len);
}