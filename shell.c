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
char **global_tokens;
char *global_input_buffer;
_Bool *global_in_background;
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
int tokenize_command(char *buff, char *tokens[])
{
    int token_count = 0;
    _Bool in_token = false;
    int num_chars = strnlen(buff, COMMAND_LENGTH);
    for (int i = 0; i < num_chars; i++)
    {
        switch (buff[i])
        {
        // Handle token delimiters (ends):
        case ' ':
        case '\t':
        case '\n':
            buff[i] = '\0';
            in_token = false;
            break;

        // Handle other characters (may be start)
        default:
            if (!in_token)
            {
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
void read_command(char *buff, char *tokens[], _Bool *in_background)
{
    *in_background = false;

    // Read input
    int length = read(STDIN_FILENO, buff, COMMAND_LENGTH - 1);

    if (length < 0 && errno != EINTR)
    {
        perror("Unable to read command from keyboard. Terminating.\n");
        exit(-1);
    }
    else if (length < 0 && errno == EINTR)
    {
        buff[0] = '\0';
    }

    // Null terminate and strip \n.
    buff[length] = '\0';
    if (buff[strlen(buff) - 1] == '\n')
    {
        buff[strlen(buff) - 1] = '\0';
    }

    // Tokenize (saving original command string)
    int token_count = tokenize_command(buff, tokens);
    if (token_count == 0)
    {
        return;
    }

    // Extract if running in background:
    if (token_count > 0 && strcmp(tokens[token_count - 1], "&") == 0)
    {
        *in_background = true;
        tokens[token_count - 1] = 0;
    }
}

// check if command is a valid external command
int is_external_command(const char *command)
{
    char *path_env = getenv("PATH");
    if (path_env == NULL)
    {
        return 0; // PATH environment variable not found
    }

    // Duplicate the PATH environment variable to avoid modifying the original
    char *path = strdup(path_env);
    if (path == NULL)
    {
        perror("strdup");
        return 0;
    }

    char *dir = strtok(path, ":");
    while (dir != NULL)
    {
        // Construct the full path to the command
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, command);

        // Check if the file exists and is executable
        if (access(full_path, X_OK) == 0)
        {
            free(path);
            return 1; // Command found and is executable
        }

        dir = strtok(NULL, ":");
    }

    free(path);
    return 0; // Command not found
}

enum CommandType
{
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
    HISTORY_RUN_PREVIOUS, // !!
    HISTORY_RUN_PREVIOUS_ERROR,
    HISTORY_CLEAR,        // !-
    HISTORY_RUN_SPECIFIC, // !<number>
    HISTORY_INVALID,
};

enum CommandType isInternalCommand(char *tokens[])
{
    if (strcmp(tokens[0], "exit") == 0)
    {
        if (tokens[1] == NULL)
        {
            return EXIT;
        }
        else
        {
            return EXIT_ERROR;
        }
    }
    if (strcmp(tokens[0], "pwd") == 0)
    {
        if (tokens[1] == NULL)
        {
            return PWD;
        }
        else
        {
            return PWD_ERROR;
        }
    }
    if (strcmp(tokens[0], "cd") == 0)
    {
        if (tokens[1] == NULL || tokens[2] == NULL)
        {
            return CD;
        }
        else
        {
            return CD_ERROR;
        }
    }
    if (strcmp(tokens[0], "help") == 0)
    {
        if (tokens[1] == NULL || tokens[2] == NULL)
        {
            return HELP;
        }
        else
        {
            return HELP_ERROR;
        }
    }
    if (strcmp(tokens[0], "history") == 0)
    {
        if (tokens[1] == NULL)
        {
            return HISTORY;
        }
        else
        {
            return HISTORY_ERROR;
        }
    }

    // if any options are passed to the history commands show an error
    if ((strcmp(tokens[0], "!!") == 0 || strcmp(tokens[0], "!-") == 0 || tokens[0][0] == '!') && tokens[1] != NULL)
    {
        return HISTORY_ERROR;
    }

    if (tokens[0][0] == '!')
    {
        if (strcmp(tokens[0], "!!") == 0)
        {
            if (get_total_commands() == 0)
            {
                return HISTORY_RUN_PREVIOUS_ERROR;
            }
            return HISTORY_RUN_PREVIOUS;
        }
        else if (strcmp(tokens[0], "!-") == 0)
        {
            return HISTORY_CLEAR;
        }
        else if (strcmp(tokens[0], "!") == 0)
        {
            return HISTORY_INVALID;
        }
        else
        {
            for (int i = 1; i < strlen(tokens[0]); i++)
            {
                if (!isdigit(tokens[0][i]))
                {
                    return HISTORY_INVALID;
                }
            }
            int id = atoi(&tokens[0][1]);
            char *command = get_command_from_history(id);
            if (command == NULL)
            {
                return HISTORY_INVALID;
            }
            return HISTORY_RUN_SPECIFIC;
        }
    }

    return NOT_INTERNAL;
}

void handle_sigint(int sig)
{
    char *help[] = {"help", NULL}; // tokenize_command(global_input_buffer, global_tokens);
    add_to_history(help);
    outputStr("\n'cd' is a builtin command for changing the current working directory.\n");
    outputStr("'exit' is a builtin command that closes the shell.\n");
    outputStr("'help' is a builtin command that provides info on all supported commands.\n");
    outputStr("'pwd' is a builtin command that displays the current working directory.\n");
}

/**
 * Main and Execute Commands
 */
int main(int argc, char *argv[])
{
    char input_buffer[COMMAND_LENGTH];
    char *tokens[NUM_TOKENS];

    global_input_buffer = input_buffer;
    global_tokens = tokens;

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0;          // 0 means we don't want to change any flag values
    sigemptyset(&sa.sa_mask); // Clear any blocked signals

    // Register signal handler
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        return 1;
    }
    while (true)
    {
        // Display prompt and read command
        char cwd[MAXPATHLEN + 1];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            size_t size = strlen(cwd);
            char termLine[size + 3]; // size of cwd, and 3 extra chars: '$', ' ', '\0'
            strcpy(termLine, cwd);
            strcat(termLine, "$ ");
            write(STDOUT_FILENO, termLine, strlen(termLine));
        }
        else
        {
            write(STDOUT_FILENO, "$ ", strlen("$ "));
        }

        _Bool in_background = false;
        read_command(input_buffer, tokens, &in_background);
        if (tokens[0] == NULL)
        {
            // no commands entered
            continue;
        }

        // Handle history commands before adding to history
        enum CommandType isInternal = isInternalCommand(tokens);
        if (isInternal == HISTORY_RUN_PREVIOUS)
        {
            run_previous_command(input_buffer);
            tokenize_command(input_buffer, tokens);
        }

        if (isInternal == HISTORY_RUN_SPECIFIC)
        {
            int id = atoi(&tokens[0][1]);
            run_command_from_history(id, input_buffer);
            tokenize_command(input_buffer, tokens);
        }

        if (isInternal == HISTORY_INVALID)
        {
            outputStr("Could not find command in history, please run a valid history command\n");
            continue;
        }

        if (isInternal == HISTORY_RUN_PREVIOUS_ERROR)
        {
            outputStr("No commands in history, can not run previous command\n");
            continue;
        }

        // Now add the executed command to history
        if (isInternal != HISTORY_ERROR && (isInternalCommand(tokens) != NOT_INTERNAL || is_external_command(tokens[0])))
        {
            add_to_history(tokens);
        }

        bool internalCommandCalled = true;
        isInternal = isInternalCommand(tokens);
        switch (isInternal)
        {
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
            if (tokens[1] == NULL) {
                chdir("/home");
                break;
            }
            if (chdir(tokens[1]) == -1)
            {
                outputStr(strerror(errno));
                outputStr("\n");
            }
            break;
        case CD_ERROR:
            outputStr("too many arguments to 'cd' call, expected 0 or 1 arguments\n");
            break;
        case HELP:
            if (tokens[1] == NULL)
            {
                outputStr("'cd' is a builtin command for changing the current working directory.\n");
                outputStr("'exit' is a builtin command that closes the shell.\n");
                outputStr("'help' is a builtin command that provides info on all supported commands.\n");
                outputStr("'pwd' is a builtin command that displays the current working directory.\n");
            }
            else
            {
                if (strcmp(tokens[1], "cd") == 0)
                {
                    outputStr("'cd' is a builtin command for changing the current working directory.\n");
                }
                else if (strcmp(tokens[1], "exit") == 0)
                {
                    outputStr("'exit' is a builtin command that closes the shell.\n");
                }
                else if (strcmp(tokens[1], "help") == 0)
                {
                    outputStr("'help' is a builtin command that provides info on all supported commands.\n");
                }
                else if (strcmp(tokens[1], "pwd") == 0)
                {
                    outputStr("'pwd' is a builtin command that displays the current working directory.\n");
                }
                else
                {
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
            outputStr("too many arguments to ");
            outputStr(tokens[0]);
            outputStr(" call, expected 0 arguments\n");
            break;
        case HISTORY_CLEAR:
            clear_history();
            break;
        case HISTORY_RUN_PREVIOUS:
            // This should not be reached
            break;
        case HISTORY_RUN_PREVIOUS_ERROR:
            break;
        case HISTORY_RUN_SPECIFIC:
            // This should not be reached
            break;
        case HISTORY_INVALID:
            // error msg output in history.c
            break;
        }

        if (internalCommandCalled)
        {
            continue;
        }

        if (in_background)
        {
            write(STDOUT_FILENO, "Run in background.", strlen("Run in background."));
        }

        // Fork and execute the command
        pid_t pid = fork();
        if (isChildProcess(pid))
        {
            if (execvp(tokens[0], tokens) == -1)
            {
                outputStr(strerror(errno));
                outputStr("\n");
            }
            return 0;
        }
        else if (isParentProcess(pid))
        {
            int status;
            if (!in_background)
            {
                if (waitpid(pid, &status, 0) == -1)
                {
                    outputStr(strerror(errno));
                    outputStr("\n");
                }
            }
        }
        else
        {
            outputStr("Failed to fork a child");
        }

        // Clean zombie processes
        while (waitpid(-1, NULL, WNOHANG) > 0)
            ; // do nothing
    }
    return 0;
}
