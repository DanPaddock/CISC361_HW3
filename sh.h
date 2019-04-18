
#include "get_path.h"

int pid;

int sh(int argc, char **argv, char **envp);

void printenv(int num_args, char **envp, char **args);

void execute_external(char** args, char* pathlist, int num_args, char** envp, int message);

char *which(char *command, struct pathelement *pathlist);

char *where(char *command, struct pathelement *pathlist);

void list(char *dir);

void run_command(int, char**, char*, int, char**);

void run_external(char**, char*, int, char**);

struct Mail_struct* mailAppend(struct Mail_struct*, char*, pthread_t);

void freeMail_struct(struct Mail_struct*);

struct Mail_struct* mailListRemoveNode(struct Mail_struct*, char*);

void mailTraverse(struct Mail_struct*);

void mailFreeAll(struct Mail_struct*);

struct User_struct* userAppend(struct User_struct*, char*);

struct User_struct* findUser(struct User_struct*, char*);

struct User_struct* userRemoveNode(struct User_struct*, char*);

void freeUser_struct(struct User_struct*);

void userFreeAll(struct User_struct*); 

#define PROMPTMAX 32
#define MAXARGS 100
