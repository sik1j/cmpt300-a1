# Define what compiler to use and the flags.
CC=cc
CXX=CC
CCFLAGS= -g -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Werror

all: shell

# Compile all .c files into .o files
%.o: %.c
	$(CC) -c $(CCFLAGS) $< -o $@

# List all object files
OBJS = shell.o history.o

# Link object files to create the executable
shell: $(OBJS)
	$(CC) -o shell $(OBJS) $(CCFLAGS)

# Clean target to remove compiled files
clean:
	rm -f core *.o shell