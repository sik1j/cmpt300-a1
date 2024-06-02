// history feature helpers
#ifndef HISTORY_H
#define HISTORY_H

void add_to_history(char **tokens);
char *get_command_from_history(int id);
void run_command_from_history(int id, char* input_buffer);
void run_previous_command(char* input_buffer);
void clear_history();
void print_history();

#endif
