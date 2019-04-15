
#include "get_path.h"

int pid;

int sh(int argc, char **argv, char **envp);

void printenv(int num_args, char **envp, char **args);

void execute_external(char** args, char* pathlist, int num_args, char** envp, int message);

char *which(char *command, struct pathelement *pathlist);

char *where(char *command, struct pathelement *pathlist);

void list(char *dir);

void run_command(int, char**, char*, int, char**);

#define PROMPTMAX 32
#define MAXARGS 100
