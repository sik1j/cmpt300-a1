// Shell starter file
// You may make any changes to any part of this file.

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include "history.h"

#include <sys/param.h>
#include <sys/wait.h>

#define COMMAND_LENGTH 1024
#define NUM_TOKENS (COMMAND_LENGTH / 2 + 1)

#define isParentProcess(pid) ((pid) > 0)
#define isChildProcess(pid) ((pid) == 0)

#define outputStr(str) write(STDOUT_FILENO, (str), strlen((str)))

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

    // If the command is a history command (starts with '!'), run the command from history
  if (tokens[0][0] == '!') {
    // check if string right after ! is a number
    for (int i = 1; i < strlen(tokens[0]); i++) {
      if (!isdigit(tokens[0][i])) {
        outputStr("Invalid history command.\n");
        return;
      }
    }
    int id = atoi(&tokens[0][1]); // convert the string after '!' to an integer
    run_command_from_history(id, buff); // run the command from history
    // Re-tokenize the command from history
    tokenize_command(buff, tokens);
  }

  // Extract if running in background:
  if (token_count > 0 && strcmp(tokens[token_count - 1], "&") == 0) {
    *in_background = true;
    tokens[token_count - 1] = 0;
  }
}

// check if command is a valid external command
int is_external_command(const char *command) {
    char *path_env = getenv("PATH");
    if (path_env == NULL) {
        return 0; // PATH environment variable not found
    }

    // Duplicate the PATH environment variable to avoid modifying the original
    char *path = strdup(path_env);
    if (path == NULL) {
        perror("strdup");
        return 0;
    }

    char *dir = strtok(path, ":");
    while (dir != NULL) {
        // Construct the full path to the command
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, command);

        // Check if the file exists and is executable
        if (access(full_path, X_OK) == 0) {
            free(path);
            return 1; // Command found and is executable
        }

        dir = strtok(NULL, ":");
    }

    free(path);
    return 0; // Command not found
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
    HISTORY,
    HISTORY_ERROR,
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
    if (strcmp(tokens[0], "history") == 0) {
        if (tokens[1] == NULL || tokens[2] == NULL) {
            return HISTORY;
        } else {
            return HISTORY_ERROR;
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

   // =========== PROBLEM 2 MAIN START ===========
    char cwd[MAXPATHLEN + 1];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        size_t size = strlen(cwd);
        char termLine[size + 3]; // size of cwd, and 3 extra chars: '$', ' ', '\0'
        strcpy(termLine, cwd);
        strcat(termLine, "$ ");
        write(STDOUT_FILENO, termLine, strlen(termLine));
    } else {
        write(STDOUT_FILENO, "$ ", strlen("$ "));
    }
    // =========== PROBLEM 2 MAIN PAUSE ===========

    _Bool in_background = false;
    read_command(input_buffer, tokens, &in_background);
    if (tokens[0] == NULL) {
        // no commands entered
        continue;
    }
    else {
        if (isInternalCommand(tokens) || is_external_command(tokens[0])) {
            add_to_history(tokens);
        }
        
    }

    // =========== PROBLEM 2 MAIN RESUME ===========
    bool internalCommandCalled = true;
    /*
     * CHECK WRITE HAS SAME STRING FOR BOTH buf AND nbyte
     *
     * */
    enum CommandType isInternal = isInternalCommand(tokens);
    switch (isInternal) {
        case NOT_INTERNAL:
            internalCommandCalled = false;
            break;
        case EXIT:
            return 0;
        case EXIT_ERROR:
            outputStr("too many arguments to 'exit' call, expected 0 arguments\n");
            break;
        case PWD:
            outputStr(cwd);
            outputStr("\n");
            break;
        case PWD_ERROR:
            outputStr("too many arguments to 'pwd' call, expected 0 arguments\n");
            break;
        case CD:
            if (chdir(tokens[1]) == -1) {
                outputStr(strerror(errno));
                outputStr("\n");
            }
            break;
        case CD_ERROR:
            outputStr("too many arguments to 'cd' call, expected 0 or 1 arguments\n");
            break;
        case HELP:
            if (tokens[1] == NULL) {
                outputStr("'cd' is a builtin command for changing the current working directory.\n");
                outputStr("'exit' is a builtin command that closes the shell.\n");
                outputStr("'help' is a builtin command that provides info on all supported commands.\n");
                outputStr("'pwd' is a builtin command that displays the current working directory.\n");
            } else {
                if (strcmp(tokens[1], "cd") == 0) {
                    outputStr("'cd' is a builtin command for changing the current working directory.\n");
                } else if (strcmp(tokens[1], "exit") == 0) {
                    outputStr("'exit' is a builtin command that closes the shell.\n");
                } else if (strcmp(tokens[1], "help") == 0) {
                    outputStr("'help' is a builtin command that provides info on all supported commands.\n");
                } else if (strcmp(tokens[1], "pwd") == 0) {
                    outputStr("'pwd' is a builtin command that displays the current working directory.\n");
                } else {
                    outputStr("'");
                    outputStr(tokens[1]);
                    outputStr("' is an external command or application\n");
                }
            }
            break;
        case HELP_ERROR:
            outputStr("too many arguments to 'help' call, expected 0 or 1 arguments\n");
            break;
        case HISTORY:
            print_history();
            break;
        case HISTORY_ERROR:
            outputStr("too many arguments to 'history' call, expected 0 arguments\n");
            break;
    }

    if (internalCommandCalled) {
        continue;
    }

    // =========== PROBLEM 2 MAIN END ===========

    /**
     * Steps For Basic Shell:
     * 1. Fork a child process
     * 2. Child process invokes execvp() using results in token array.
     * 3. If in_background is false, parent waits for
     *    child to finish. Otherwise, parent loops back to
     *    read_command() again immediately.
     */

    // =========== PROBLEM 1 MAIN START ===========
    pid_t pid = fork();
    if (isChildProcess(pid)) {
        if (execvp(tokens[0], tokens) == -1) {
            outputStr(strerror(errno));
            outputStr("\n");
        }
        return 0;
    } else if (isParentProcess(pid)) {
        int status;
        if (!in_background) {
            // What in the hell does STATUS mean?
            if (waitpid(pid, &status, 0) == -1) {
                outputStr(strerror(errno));
                outputStr("\n");
            }
        }
    } else {
        outputStr("Failed to fork a child");
    }

    // Clean zombie processes
      while (waitpid(-1, NULL, WNOHANG) > 0)
          ; // do nothing
    // =========== PROBLEM 1 MAIN END ===========
  }
  return 0;
}
