// history feature helpers
#ifndef HISTORY_H
#define HISTORY_H

void add_to_history(char **tokens);
char *get_command_from_history(int id);
void print_history();

#endif
