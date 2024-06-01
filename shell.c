// Shell starter file
// You may make any changes to any part of this file.

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/param.h>

#include <errno.h>

#define COMMAND_LENGTH 1024
#define NUM_TOKENS (COMMAND_LENGTH / 2 + 1)

/**
 * Command Input and Processing
 */

/*
 * Tokenize the string in 'buff' into 'tokens'.
 * buff: Character array containing string to tokenize.
 *       Will be modified: all whitespace replaced with '\0'
 * tokens: array of pointers of size at least COMMAND_LENGTH/2 + 1.
 *       Will be modified so tokens[i] points to the i'th token
 *       in the string buff. All returned tokens will be non-empty.
 *       NOTE: pointers in tokens[] will all point into buff!
 *       Ends with a null pointer.
 * returns: number of tokens.
 */
int tokenize_command(char *buff, char *tokens[]) {
  int token_count = 0;
  _Bool in_token = false;
  int num_chars = strnlen(buff, COMMAND_LENGTH);
  for (int i = 0; i < num_chars; i++) {
    switch (buff[i]) {
    // Handle token delimiters (ends):
    case ' ':
    case '\t':
    case '\n':
      buff[i] = '\0';
      in_token = false;
      break;

    // Handle other characters (may be start)
    default:
      if (!in_token) {
        tokens[token_count] = &buff[i];
        token_count++;
        in_token = true;
      }
    }
  }
  tokens[token_count] = NULL;
  return token_count;
}

/**
 * Read a command from the keyboard into the buffer 'buff' and tokenize it
 * such that 'tokens[i]' points into 'buff' to the i'th token in the command.
 * buff: Buffer allocated by the calling code. Must be at least
 *       COMMAND_LENGTH bytes long.
 * tokens[]: Array of character pointers which point into 'buff'. Must be at
 *       least NUM_TOKENS long. Will strip out up to one final '&' token.
 *       tokens will be NULL terminated (a NULL pointer indicates end of
 * tokens). in_background: pointer to a boolean variable. Set to true if user
 * entered an & as their last token; otherwise set to false.
 */
void read_command(char *buff, char *tokens[], _Bool *in_background) {
  *in_background = false;

  // Read input
  int length = read(STDIN_FILENO, buff, COMMAND_LENGTH - 1);

  if (length < 0) {
    perror("Unable to read command from keyboard. Terminating.\n");
    exit(-1);
  }

  // Null terminate and strip \n.
  buff[length] = '\0';
  if (buff[strlen(buff) - 1] == '\n') {
    buff[strlen(buff) - 1] = '\0';
  }

  // Tokenize (saving original command string)
  int token_count = tokenize_command(buff, tokens);
  if (token_count == 0) {
    return;
  }

  // Extract if running in background:
  if (token_count > 0 && strcmp(tokens[token_count - 1], "&") == 0) {
    *in_background = true;
    tokens[token_count - 1] = 0;
  }
}

enum CommandType {
    NOT_INTERNAL,
    EXIT,
    EXIT_ERROR,
    PWD,
    PWD_ERROR,
    CD,
    CD_ERROR,
    HELP,
    HELP_ERROR,
};

enum CommandType isInternalCommand(char *tokens[]) {
    if (strcmp(tokens[0], "exit") == 0) {
        if (tokens[1] == NULL) {
            return EXIT;
        } else {
            return EXIT_ERROR;
        }
    }
    if (strcmp(tokens[0], "pwd") == 0) {
        if (tokens[1] == NULL) {
            return PWD;
        } else {
            return PWD_ERROR;
        }
    }
    if (strcmp(tokens[0], "cd") == 0) {
        if (tokens[1] == NULL || tokens[2] == NULL) {
            return CD;
        } else {
            return CD_ERROR;
        }
    }
    if (strcmp(tokens[0], "help") == 0) {
        if (tokens[1] == NULL || tokens[2] == NULL) {
            return HELP;
        } else {
            return HELP_ERROR;
        }
    }

    return NOT_INTERNAL;
}

/**
 * Main and Execute Commands
 */
int main(int argc, char *argv[]) {
  char input_buffer[COMMAND_LENGTH];
  char *tokens[NUM_TOKENS];
  while (true) {

    // Get command
    // Use write because we need to use read() to work with
    // signals, and read() is incompatible with printf().
    char cwd[MAXPATHLEN+3]; // path size + 3 extra chars ($, <SPACE>, \0)
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        size_t size = strlen(cwd);
        char termLine[size + 3];
        strcpy(termLine, cwd);
        strcat(termLine, "$ ");
        write(STDOUT_FILENO, termLine, strlen(termLine));
    } else {
        write(STDOUT_FILENO, "$ ", strlen("$ "));
    }

    _Bool in_background = false;
    read_command(input_buffer, tokens, &in_background);

    // DEBUG: Dump out arguments:
    for (int i = 0; tokens[i] != NULL; i++) {
      write(STDOUT_FILENO, "   Token: ", strlen("   Token: "));
      write(STDOUT_FILENO, tokens[i], strlen(tokens[i]));
      write(STDOUT_FILENO, "\n", strlen("\n"));
    }


    /*
     * CHECK WRITE HAS SAME STRING FOR BOTH buf AND nbyte
     *
     * */
    enum CommandType isInternal = isInternalCommand(tokens);
    switch (isInternal) {
        case NOT_INTERNAL:
            break;
        case EXIT:
            return 0;
        case EXIT_ERROR:
            write(STDOUT_FILENO, "too many arguments to 'exit' call, expected 0 arguments\n", strlen("too many arguments to 'exit' call, expected 0 arguments\n"));
            break;
        case PWD:
            write(STDOUT_FILENO, cwd, strlen(cwd));
            write(STDOUT_FILENO, "\n", strlen("\n"));
            break;
        case PWD_ERROR:
            write(STDOUT_FILENO, "too many arguments to 'pwd' call, expected 0 arguments\n", strlen("too many arguments to 'pwd' call, expected 0 arguments\n"));
            break;
        case CD:
            if (chdir(tokens[1]) == -1) {
                write(STDOUT_FILENO, strerror(errno), strlen(strerror(errno)));
                write(STDOUT_FILENO, "\n", strlen("\n"));
            }
            break;
        case CD_ERROR:
            write(STDOUT_FILENO, "too many arguments to 'cd' call, expected 0 or 1 arguments\n", strlen("too many arguments to 'cd' call, expected 0 or 1 arguments\n"));
            break;
        case HELP:
            if (tokens[1] == NULL) {
                write(STDOUT_FILENO, "'cd' is a builtin command for changing the current working directory.\n", strlen("'cd' is a builtin command for changing the current working directory.\n"));
                write(STDOUT_FILENO, "'exit' is a builtin command that closes the shell.\n", strlen("'exit' is a builtin command that closes the shell.\n"));
                write(STDOUT_FILENO, "'help' is a builtin command that provides info on all supported commands.\n", strlen("'help' is a builtin command that provides info on all supported commands.\n"));
                write(STDOUT_FILENO, "'pwd' is a builtin command that displays the current working directory.\n", strlen("'pwd' is a builtin command that displays the current working directory.\n"));
            } else {
                if (strcmp(tokens[1], "cd") == 0) {
                    write(STDOUT_FILENO, "'cd' is a builtin command for changing the current working directory.\n", strlen("'cd' is a builtin command for changing the current working directory.\n"));
                } else if (strcmp(tokens[1], "exit") == 0) {
                    write(STDOUT_FILENO, "'exit' is a builtin command that closes the shell.\n", strlen("'exit' is a builtin command that closes the shell.\n"));
                } else if (strcmp(tokens[1], "help") == 0) {
                    write(STDOUT_FILENO, "'help' is a builtin command that provides info on all supported commands.\n", strlen("'help' is a builtin command that provides info on all supported commands.\n"));
                } else if (strcmp(tokens[1], "pwd") == 0) {
                    write(STDOUT_FILENO, "'pwd' is a builtin command that displays the current working directory.\n", strlen("'pwd' is a builtin command that displays the current working directory.\n"));
                } else {
                    write(STDOUT_FILENO, "'", strlen("'"));
                    write(STDOUT_FILENO, tokens[1], strlen(tokens[1]));
                    write(STDOUT_FILENO, "' is an external command or application\n", strlen("' is an external command or application\n"));
                }
            }
            break;
        case HELP_ERROR:
            write(STDOUT_FILENO, "too many arguments to 'help' call, expected 0,1 arguments\n", strlen("too many arguments to 'help' call, expected 0,1 arguments\n"));
            break;
    }

    if (in_background) {
      write(STDOUT_FILENO, "Run in background.", strlen("Run in background."));
    }

    /**
     * Steps For Basic Shell:
     * 1. Fork a child process
     * 2. Child process invokes execvp() using results in token array.
     * 3. If in_background is false, parent waits for
     *    child to finish. Otherwise, parent loops back to
     *    read_command() again immediately.
     */
  }
  return 0;
}
