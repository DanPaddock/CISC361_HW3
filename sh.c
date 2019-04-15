#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <stdlib.h>
#include <pwd.h>
#include "sh.h"
#include <glob.h>
#include "linked_list.h"
#include <utmpx.h>
#include <assert.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define buf_size 1000

int uid, i, status, argsct, go = 1;
struct passwd *password_entry;
char *homedir;
struct pathelement *pathlist;
char *command, *arg, *commandpath, *p, *pwd, *owd, *cwd;
char *prompt_prefix = NULL;
int len;
char buf[buf_size];

void handle_sigchild(int sig) {
    while (waitpid((pid_t) (-1), 0, WNOHANG) > 0) {}
}

char *commands[] = {
    "exit",
    "which",
    "where",
    "cd",
    "pwd",
    "list",
    "pid",
    "kill",
    "prompt",
    "printenv",
    "setenv"
};

int sh(int argc, char **argv, char **envp) {
    
    prompt_prefix = (char *) malloc(0);
    char *prompt = calloc(PROMPTMAX, sizeof(char));
    char **args = calloc(MAXARGS, sizeof(char *));
    uid = getuid();
    password_entry = getpwuid(uid);
    homedir = password_entry->pw_dir;

    if ((pwd = getcwd(buf, buf_size + 1)) == NULL) {
        perror("getcwd");
        exit(2);
    }

    cwd = calloc(strlen(pwd) + 1, sizeof(char));
    owd = calloc(strlen(pwd) + 1, sizeof(char));
    memcpy(owd, pwd, strlen(pwd));
    memcpy(cwd, pwd, strlen(pwd));
    prompt[0] = ' ';
    prompt[1] = '\0';

    pathlist = get_path();

    int len;
    char *string_input;

    sigignore(SIGINT);
    sigignore(SIGTERM);
    sigignore(SIGTSTP);
    signal(SIGCHLD, handle_sigchild);

    while (go) {

        for (int j = 0; j < MAXARGS; j++) {
            args[j] = NULL;
        }

        printf("%s[%s]>> ", prompt_prefix, cwd);

        fgets(buf, buf_size, stdin);
        len = (int) strlen(buf);

        if (len >= 2) {
            buf[len - 1] = '\0';
            string_input = (char *) malloc(len);
            char *input = (char *) malloc(len);
            strcpy(string_input, buf);
            strcpy(input, buf);

            char * pch;
            char * command = "";
            pch = strtok (buf," ");
            int args_len = 0;
            while (pch != NULL)
            {
                if(command == ""){
                    command = pch;
                }
                args[args_len] = (char *) malloc(len);
                args[args_len] = pch;
                args_len++;
                pch = strtok (NULL, " ");
                args[args_len] = (char *) malloc(len);
            }

            int piped = 0;
            int pipeindex = -1;
            int pipe_error = 0;

            for(int i = 0; i < args_len; i++){
                if(strcmp(args[i], "|") == 0 || strcmp(args[i], "|&") == 0){
                    if(strcmp(args[i], "|&") == 0){
                        pipe_error = 1;
                    }
                    piped = 1;
                    pipeindex = i;
                    break;
                }
            }

            char** piped_args = NULL;
            int piped_arg_nums = 0;

            int pfds[2];
            int first_child = -1;
            int second_child = -1;
            int command_index = 99;
            int flag = 0;
            
            for(int ind = 0; ind < 11; ind++){
                if(strcmp(commands[ind], command) == 0){
                    command_index = ind;
                }
            }

            if(piped){
                char** piped_args = calloc(MAXARGS, sizeof(char *));

                for(int i = 0; i < MAXARGS; i++){
                    piped_args[i] = NULL;
                }
                
                free(args[pipeindex]);

                for(int i = pipeindex+1; i < MAXARGS; i++){
                    if(args[i] != NULL){
                        piped_args[i-pipeindex-1] = args[i];
                        piped_arg_nums++;
                    }else{
                        break;
                    }
                }

                for(int j = pipeindex; j < MAXARGS; j++) {
                    if(args[j] == NULL){
                        break;
                    }
                    args[j] = NULL;
                    args_len--;
                }

                pipe(pfds);
                first_child = fork();

                if(first_child == 0){
                    if(pipe_error){
                        close(STDERR_FILENO);
                        dup(pfds[1]);
                    }

                    close(STDOUT_FILENO);
                    dup(pfds[1]);
                    close(pfds[0]);
                    run_command(command_index, args, pathlist, args_len, envp);

                    exit(0);
                }else{
                    second_child = fork();
                    if(second_child == 0){
                        close(STDIN_FILENO);
                        dup(pfds[0]);
                        close(pfds[1]);
                        exit(0);
                    }else{
                        close(pfds[0]);
                        close(pfds[1]);
                        int c_status;
                        waitpid(second_child, &c_status, 0);

                        for (int j = 0; j < MAXARGS; j++) {
                            if(piped_args[j] != NULL){
                                free(piped_args[j]);
                            }
                        }
                        free(piped_args);
                    }
                }
               
            }
    
                if(piped == 0)
                    run_command(command_index, args, pathlist, args_len, envp);
            }
        }
    
    free(prompt);
    free(owd);
    free(cwd);
    free(args);
    free(prompt_prefix);

    struct pathelement *current;
    current = pathlist;
    free(current->element);

    while (current != NULL) {
        free(current);
        current = current->next;
    }

    return 0;
} /* sh() */

void run_command(int command_index, char** args, char* pathlist, int args_len, char** envp){
    switch (command_index) {
        case 0: // exit
            go = 0;
            break;
        case 1: // which
            if (args_len == 1) {
                printf("Too few arguments... Usage: which [file_name]\n");
            } else {
                for (int i = 1; i < args_len; i++) {
                    if (args[i] != NULL) {
                        char *result = which(args[i], pathlist);
                        if (result != NULL) {
                            printf("%s\n", result);
                            free(result);
                        } else {
                            printf("%s not found\n", args[i]);
                        }
                    } else {
                        break;
                    }
                }
            }
            break;
        case 2: // where
            if (args_len == 1) {
                printf("Too few arguments... Usage: where [file_name]\n");
            } else {
                for (int i = 1; i < args_len; i++) {
                    if (args[i] != NULL) {
                        char *result = where(args[i], pathlist);
                        if (result != NULL) {
                            printf("%s\n", result);
                            free(result);
                        } else {
                            printf("%s not found\n", args[i]);
                        }
                    } else {
                        break;
                    }
                }
            }
            break;
        case 3: // cd
            printf("");
            char *cd_path = args[1];

            if (args_len > 2) {
                perror("Too few arguments... Usage: cd [directory]\n");
            } else {
                if (args_len == 1) {
                    cd_path = homedir;
                } else if (args_len == 2) {
                    cd_path = args[1];
                }

                if ((pwd = getcwd(buf, buf_size + 1)) == NULL) {
                    perror("getcwd");
                    exit(2);
                }

                if (cd_path[0] == '-') {
                    if (chdir(owd) < 0) {
                        printf("Directory Not Found: %d\n", errno);
                    } else {
                        free(cwd);
                        cwd = malloc((int) strlen(owd));
                        strcpy(cwd, owd);


                        free(owd);
                        owd = malloc((int) strlen(buf));
                        strcpy(owd, buf);
                    }
                } else {
                    if (chdir(cd_path) < 0) {
                        printf("Directory Not Found: %d\n", errno);
                    } else {
                        free(owd);
                        owd = malloc((int) strlen(buf));
                        strcpy(owd, buf);

                        if ((pwd = getcwd(buf, buf_size + 1)) == NULL) {
                            perror("getcwd");
                            exit(2);
                        }

                        free(cwd);
                        cwd = malloc((int) strlen(buf));
                        strcpy(cwd, buf);
                    }
                }
            }
            break;
        case 4: // pwd
            printf("%s\n", cwd);
            break;
        case 5: // list
            if (args_len == 1) {
                list(cwd);
            } else {
                for (int i = 1; i < MAXARGS; i++) {
                    if (args[i] != NULL) {
                        printf("[%s]:\n", args[i]);
                        list(args[i]);
                    }
                }
            }
            break;
        case 6: // pid
            printf("");
            int pid = getpid();
            printf("%d\n", pid);
            break;
        case 7: // kill
            if (args_len == 3) {
                char *pid_str = args[2];
                char *signal_str = args[1];

                char *end;
                long pid_num;
                long sig_num;
                pid_num = strtol(pid_str, &end, 10);
                
                if (end == pid_str) {
                    printf("%s\n", "Conversion Error!");
                }
                signal_str[0] = ' ';
                sig_num = strtol(signal_str, &end, 10);

                if (end == signal_str) {
                    printf("%s\n", "Conversion Error!");
                }

                int id = (int) pid_num;
                int sig = (int) sig_num;
                kill(id, sig_num);
            } else if (args_len == 2) {
                char *pid_str = args[1];
                char *end;
                long num;
                num = strtol(pid_str, &end, 10);
                if (end == pid_str) {
                    printf("%s\n", "Conversion Error!");
                }
                int id = (int) num;
                kill(id, SIGTERM);
            } else {
                printf("Invalid Arguments... Usage: kill [number]");
            }
            break;
        case 8: // prompt
            free(prompt_prefix);
            if (args_len == 1) {
                fgets(buf, buf_size, stdin);
                len = (int) strlen(buf);
                buf[len - 1] = '\0';
                prompt_prefix = (char *) malloc(len);
                strcpy(prompt_prefix, buf);
            } else if (args_len == 2) {
                prompt_prefix = (char *) malloc(strlen(args[1]));
                strcpy(prompt_prefix, args[1]);
            }
            break;
        case 9: // printenv
            printenv(args_len, envp, args);
            break;
        case 10: // setenv
            if (args_len == 1) {
                printenv(args_len, envp, args);
            } else if (args_len == 2) {
                setenv(args[1], "", 1);
            } else if (args_len == 3) {
                setenv(args[1], args[2], 1);
                if (strcmp(args[1], "homedir") == 0) {
                    homedir = getenv("homedir");
                } else if (strcmp(args[1], "PATH") == 0) {
                    pathlist = get_path();
                }
            } else {
                printf("Too few arguments... Usage: setenv [arguments]\n");
            }
            break;
        default:
             printf("Command [%s] does not exist!\n", command);
    }
}

void printenv(int args_len, char **envp, char **args) {
    if (args_len == 1) {
        int i = 0;
        while (envp[i] != NULL) {
            printf("%s\n", envp[i]);
            i++;
        }
    } else if (args_len == 2) {
        char *env_str = getenv(args[1]);
        if (env_str != NULL) {
            printf("%s\n", env_str);
        }
    }
}


char *which(char *command, struct pathelement *pathlist) {
    char e_buf[buf_size];
    struct pathelement *current = pathlist;
    DIR *dr;
    struct dirent *de;
    while (current != NULL) {

        char *path = current->element;

        dr = opendir(path);

        if(dr){

            while ((de = readdir(dr)) != NULL) {

                if (strcmp(de->d_name, command) == 0) {
                    strcpy(e_buf, path);
                    strcat(e_buf, "/");
                    strcat(e_buf, de->d_name);

                    int len = (int) strlen(e_buf);
                    char *p = (char *) malloc(len);
                    strcpy(p, e_buf);

                    closedir(dr);

                    return p;
                }
            }
        }
        closedir(dr);
        current = current->next;
    }
    return NULL;

}

char *where(char *command, struct pathelement *pathlist) {
    char e_buf[buf_size];
    struct pathelement *current = pathlist;

    DIR *dr;
    struct dirent *de;
    strcpy(e_buf, "");
    while (current != NULL) {
        char *path = current->element;
        dr = opendir(path);
        if(dr){
            while ((de = readdir(dr)) != NULL) {
                if (strcmp(de->d_name, command) == 0) {
                    strcat(e_buf, path);
                    strcat(e_buf, "/");
                    strcat(e_buf, de->d_name);
                    strcat(e_buf, "\n");
                }
            }
        }
        closedir(dr);
        current = current->next;
    }

    int len = (int) strlen(e_buf);
    char *p = (char *) malloc(len);
    e_buf[len - 1] = '\0';
    strcpy(p, e_buf);

    return p;
} /* where() */

void list(char *dir) {

    DIR *dr;
    struct dirent *de;
    dr = opendir(dir);
    if (dr == NULL) {
        printf("Cannot open %s\n", dir);
    } else {
        while ((de = readdir(dr)) != NULL) {
            printf("%s\n", de->d_name);
        }
    }


    closedir(dr);
} /* list() */

