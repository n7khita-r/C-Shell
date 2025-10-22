// the code in this file defines a context free grammar lexer and
// parser for boolean algebra statements. it is a very simple grammar,
// and thus does not implement features such as operator precedence.
// note that this program also does not execute the statements, merely
// lexes and parses them in a simple, easy-to-understand manner.

// the file is filled with comments that explain each part of the code.
// it is best to first run the code, play around with the syntax a bit,
// and then scroll down all the way to the `main` function and start
// going up one function at a time. the functions get narrower in their
// scope the higher up you go in the code. if you are unsure of what
// some part of the code does, use `gdb` to pause execution and explore
// the values of each variable at every step.

// to compile and run this program, execute the following commands:
//
//     gcc cfg.c --output cfg && ./cfg
//
// this should start the program and wait for your input.

// the context free grammar that this lexer + parser understands is:
//
// statement  ->   assignment | expression
// assignment ->   variable = expression
// variable   ->   [a-zA-Z_][a-zA-Z0-9_]*
// expression ->   variable | TRUE | FALSE
//                 | ( expression )
//                 | NOT expression
//                 | expression AND expression
//                 | expression OR expression
//
// the string representing a variable must obey the above regex.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

// define an enum with the possible tokens we can parse from the
// input string - a variable, a true/false constant, a not/and/or
// operator, an assignment or a parenthesis, or the end of input.
typedef enum {
  token_eof, token_error,
  token_var, token_true, token_false,
  token_not, token_and, token_or,
  token_assign, token_lparen, token_rparen
} token_kind;

// defines the states the parser can be in (since the parser is a state
// machine after all) - see the `parse` function for more info.
typedef enum {
  expect_identifier,
  expect_operator
} parse_state;

// define a struct to hold information of a token encountered while
// parsing the input - the kind, where it was found, and its length.
typedef struct {
  token_kind kind;
  const char* start;
  int length;
} token;

// define a struct to hold the current state of the lexer, i.e., the
// input it is processing, the start of the current token, and the
// current position in the input that the programming is inspecting.
typedef struct {
  const char* input;
  const char* start;
  const char* current;
} lexer;

// define a struct to hold the current state of the parser. this is
// the object we will pass around to all the functions, so it has a
// reference to the lexer being used, as well as the current and
// previously parsed token. the error flag is set when an error is
// encountered.
typedef struct {
  lexer lexer;
  token current;
  token previous;
  bool error;
} parser;

// checks whether or not we have reached the end of the user's input.
// assumes that the input string will be null-terminated.
static bool found_eof(parser* p) {
  return *p->lexer.current == '\0';
}

// creates a new token from the current state of the parser, of the
// given type. returns the new token struct.
static token make_token(parser* p, token_kind kind) {
  token token;
  token.kind = kind;
  token.start = p->lexer.start;
  token.length = (int) (p->lexer.current - p->lexer.start);
  return token;
}

// creates a special error token to use when an error is detected in
// the input. this token is of type `token_error`, and the `start`
// property of the struct is actually a pointer to the error message.
static token error_token(const char* message) {
  token token;
  token.kind = token_error;
  token.start = message;
  token.length = (int) strlen(message);
  return token;
}

// if the cursor of the lexer is at a whitespace character (defined as
// a space, tab, newline or carriage return), move it ahead.
static void skip_whitespace(parser* p) {
  while (true) {
    switch (*p->lexer.current) {
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        p->lexer.current++;
        break;
      default:
        return;
    }
  }
}

// this function helps determine if a word is a keyword or a variable.
// it checks if the length and content of the current token match the
// provided keyword.
static token_kind check_keyword(
  parser* p, int start, int length,
  const char* rest, token_kind kind
) {
  if (p->lexer.current - p->lexer.start == start + length &&
    memcmp(p->lexer.start + start, rest, length) == 0
  ) return kind;
  return token_var;
}

// for keywords such as `and`, `or`, `not`, `true`, `false`, etc. we
// need to figure out if the word is a keyword or a variable name.
// this is done by checking the token against all possible keywords.
static token make_identifier(parser* p) {
  while (
    isalpha(*p->lexer.current) ||
    isdigit(*p->lexer.current) ||
    *p->lexer.current == '_'
  ) p->lexer.current++;

  token_kind kind = token_var;
  switch (p->lexer.start[0]) {
    case 'a': kind = check_keyword(p, 1, 2, "nd", token_and); break;
    case 'f': kind = check_keyword(p, 1, 4, "alse", token_false); break;
    case 'n': kind = check_keyword(p, 1, 2, "ot", token_not); break;
    case 'o': kind = check_keyword(p, 1, 1, "r", token_or); break;
    case 't': kind = check_keyword(p, 1, 3, "rue", token_true); break;
  }
  return make_token(p, kind);
}

// moves the cursor ahead in search of the next token, marshalls the
// token it finds into a struct of the right kind, and returns it.
static token next_token(parser* p) {
  skip_whitespace(p);
  p->lexer.start = p->lexer.current;

  if (found_eof(p)) return make_token(p, token_eof);

  char c = *p->lexer.current++;
  if (isalpha(c) || c == '_') return make_identifier(p);

  switch (c) {
    case '(': return make_token(p, token_lparen);
    case ')': return make_token(p, token_rparen);
    case '=': return make_token(p, token_assign);
  }

  return error_token("unexpected character.");
}

// this prints a formatted error message, pointing to the location
// of the token that caused the parse to fail.
static void report_error(parser *p, const char* message) {
  fprintf(stderr, "Parse Error: %s\n\n", message);
  fprintf(stderr, "    %s", p->lexer.input);

  size_t len = strlen(p->lexer.input);
  if (len == 0 || p->lexer.input[len - 1] != '\n')
    fprintf(stderr, "\n");

  int column = p->previous.start - p->lexer.input;
  fprintf(stderr, "    %*s", column, "");
  fprintf(stderr, "^");
  for (int i = 1; i < p->previous.length; i++)
    fprintf(stderr, "~");
  fprintf(stderr, "\n");
}

// sets the parser's error flag and prints the reported error. we only
// report the first error we find to avoid confusing cascades.
static void parse_error(parser* p, const char* message) {
  if (p->error) return;
  p->error = true;
  report_error(p, message);
}

// move the parser one step forward by fetching the next token from the
// lexer. if the token is an error token, it displays the error.
static void advance_parser(parser* p) {
  p->previous = p->current; p->current = next_token(p);
  if (p->current.kind == token_error)
    parse_error(p, p->current.start);
}

// checks if the current token is of the expected kind.
static bool check_token(parser* p, token_kind kind) {
  return p->current.kind == kind;
}

// if the current token matches the expected kind, it consumes the
// token by calling `advance` and returns true. otherwise, it returns
// false without consuming the token, so we can try another `match`.
static bool match_token(parser* p, token_kind kind) {
  if (!check_token(p, kind)) return false;
  advance_parser(p); return true;
}

// sets up the parser struct with the input to be parsed.
static void create_parser(parser* p, const char* source) {
  p->lexer.input = source;
  p->lexer.start = source;
  p->lexer.current = source;
  p->current.kind = token_error;
  p->previous.kind = token_error;
  p->error = false;
}

// the parse function operates as a state machine of sorts, with two
// states (expect_identifier and expect_operator), and a memory that
// keeps track of the current bracket nesting level. read the comments
// accompanying the code within the function to understand how the
// parsing works.
bool parse_input(const char* source) {
  // create a parser and fetch the first token.
  parser p; create_parser(&p, source); advance_parser(&p);

  // start out expecting an identifier (a variable or keyword like true)
  // and with 0 nested parenthesis.
  parse_state state = expect_identifier; int paren_level = 0;

  // first, check for an assignment statement - it must be a variable,
  // followed by '=', followed by an expression. to parse the actual
  // expression, we switch to expect_identifier mode. if it is not an
  // assignment, but the current token is indeed a variable, we switch
  // to finding an operator to act on the variable.
  if (match_token(&p, token_var)) {
    if (match_token(&p, token_assign)) state = expect_identifier;
    else state = expect_operator;
  }

  // enter the main parsing loop. it continues as long as the cursor
  // does not reach the end of the input and no errors have been found.
  // in the loop, we check what state the machine is in, and deal with
  // the encountered tokens accordingly. some tokens may also result in
  // a change in the state of the parser.
  while (!check_token(&p, token_eof) && !p.error) {
    if (state == expect_identifier) {
      if (match_token(&p, token_not)) {
        // `not` is a prefix, so we still expect a primary next.
        state = expect_identifier;
      } else if (match_token(&p, token_lparen)) {
        // if we encounter a parenthesis, increase the nested paren
        // level and keep expecting an identifier.
        paren_level++; state = expect_identifier;
      } else if (
        match_token(&p, token_var) ||
        match_token(&p, token_true) ||
        match_token(&p, token_false)
      ) {
        // the current token is an identifier, so the next must be an
        // operator. set the state, and move to the next token.
        state = expect_operator;
      } else {
        // if it is anything else, it's a syntax error.
        parse_error(&p, "expected a variable, literal, 'not', or '('.");
      }

      continue;
    }

    if (state == expect_operator) {
      if (match_token(&p, token_and) || match_token(&p, token_or)) {
        // after an `and` or `or` operator, we need another identifier
        // since the operators are binary.
        state = expect_identifier;
      } else if (match_token(&p, token_rparen)) {
        if (paren_level <= 0) {
          // if there are more closing parens than opening parens, that
          // is also a syntax error.
          parse_error(&p, "unmatched closing parenthesis ')'.");
        } else {
          // after a paren, we still expect another operator, since the
          // paren just closed an expression.
          paren_level--; state = expect_operator;
        }
      } else {
        // if we see anything else, it's an error.
        parse_error(
          &p, "expected 'and', 'or', ')', or end of statement."
        );
      }

      continue;
    }
  }

  // if there are no errors, check if there are no pending states to
  // clear. for example, if we still have open parenthesis, or if the
  // statement ended right after an operator like 'and'.
  if (!p.error) {
    if (paren_level > 0)
      parse_error(&p, "unmatched opening parenthesis '('.");
    else if (state == expect_identifier) {
      parse_error(&p, "expression cannot end with an operator.");
    }
  }

  // if there truly are no errors, return true, since this was parsed
  // successfully. typically in an actual program you would return an
  // array of whatever tokens have been parsed, or convert the tokens
  // into an array of statements that can be executed and return that.
  return !p.error;
}

// the main function is a simple read-eval-print loop that puts our
// lexer + parser into action. it prints `ok` if the input was valid
// according to the cfg our parser implements, and an error if not.
int main() {
  printf("tiny parser for boolean algebra\n");
  printf("enter an empty line or press ctrl+d to exit.\n");

  char line[1024];
  while (true) {
    printf("\n> ");
    if (!fgets(line, sizeof(line), stdin) || *line == '\n') break;
    if (parse_input(line)) printf("ok");
  }

  return 0;
}
