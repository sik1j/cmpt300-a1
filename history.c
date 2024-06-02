// command history feature
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "history.h"

#define HISTORY_DEPTH 10
#define COMMAND_LENGTH 1024
#define outputStr(str) write(STDOUT_FILENO, (str), strlen((str)))

// command history and each command has its own id
char history[HISTORY_DEPTH][COMMAND_LENGTH];
int total_commands = 0;

// add command to history
void add_to_history(char** tokens) {
    int index = total_commands % HISTORY_DEPTH;
    char buffer[COMMAND_LENGTH] = ""; // Initialize buffer to empty string

    // Concatenate all tokens into buffer
    for (int i = 0; tokens[i] != NULL; i++) {
        strncat(buffer, tokens[i], COMMAND_LENGTH - strlen(buffer) - 1);
        strncat(buffer, " ", COMMAND_LENGTH - strlen(buffer) - 1); // Add space between tokens
    }

    // Copy buffer to history
    strncpy(history[index], buffer, COMMAND_LENGTH);
    history[index][COMMAND_LENGTH - 1] = '\0'; // Ensure null-termination
    total_commands++;
}

// retrieve command
char* get_command_from_history(int id) {
    if (id < 0 || id >= total_commands) {
        return NULL;
    }

    int index = id % HISTORY_DEPTH;
    return history[index];
}

// run command from history with !n to run command number n
void run_command_from_history(int id, char* input_buffer) {
    char* command = get_command_from_history(id);
    if (command != NULL) {
        outputStr(command);
        outputStr("\n");
        strncpy(input_buffer, command, COMMAND_LENGTH);
    } else {
        outputStr("Command not found in history\n");
    }
}

// run previous command
void run_previous_command(char* input_buffer) {
    run_command_from_history(total_commands - 1, input_buffer);
}

// clear history
void clear_history() {
    total_commands = 0;
}

// print last 10 commands to the screen
void print_history() {
    char buffer[COMMAND_LENGTH + 20]; // Extra space for command number and newline
    int start = total_commands > HISTORY_DEPTH ? total_commands - HISTORY_DEPTH : 0;

    for (int i = total_commands - 1; i >= start; i--) {
        int index = i % HISTORY_DEPTH;
        int length = snprintf(buffer, sizeof(buffer), "%d:\t%s\n", i, history[index]);
        write(STDOUT_FILENO, buffer, length);
    }
}
