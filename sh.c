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
#include <utmpx.h>
#include <pthread.h>

#define buf_size 1000

// Structs to define
typedef struct {
    int command_index;
    char** args;
    char* pathlist;
    int args_len;
    char** envp;
} arg_struct;

struct Mail_struct
{
    char* filepath;
    pthread_t thread_id;
    struct Mail_struct *next;
};

struct User_struct
{
    char* user;
    int logged_on;
    struct User_struct *next;
};

// Declare global variables
int uid, i, status, argsct, go = 1;
struct passwd *password_entry;
struct User_struct *watchuser = NULL;
struct Mail_struct *watchmail = NULL;
char *homedir;
struct pathelement *pathlist;
char *command, *arg, *commandpath, *p, *pwd, *owd, *cwd;
char *prompt_prefix = NULL;
int len;
char buf[buf_size];
pthread_t thread;
pthread_mutex_t mutex, mutex_user;
int noclobber = 0;
pthread_t watchuser_tid;
int watching_users = 0;

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
    "setenv",
    "noclobber",
    "watchuser",
    "watchmail"
};

// Main Function to handle everything
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
            pthread_mutex_init(&mutex, NULL);
            char * pch;
            char * RequestMT = "";
            char * command = "";
            pch = strtok (buf," ");
            int args_len = 0;
            while (pch != NULL)
            {
                RequestMT = pch;
                if(command == ""){
                    command = pch;
                }
                if(strcmp(pch,"&") != 0){
                    args[args_len] = (char *) malloc(len);
                    args[args_len] = pch;
                    args_len++;
                    pch = strtok (NULL, " ");
                    args[args_len] = (char *) malloc(len);
                }
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
            
            for(int ind = 0; ind < 14; ind++){
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
                    
                    if (strcmp(RequestMT,"&") == 0){
                        printf("HEre.....");
                        arg_struct args_pass = {
                            .command_index = command_index,
                            .args = args,
                            .pathlist = pathlist,
                            .args_len = args_len,
                            .envp = envp
                        };
            
                        pthread_create(&thread, NULL, run_command, (void*)&args_pass);
                    } else run_command(command_index, args, pathlist, args_len, envp);

                    exit(0);
                }else{
                    second_child = fork();
                    if(second_child == 0){
                        close(STDIN_FILENO);
                        dup(pfds[0]);
                        close(pfds[1]);
                        run_external(args, pathlist, args_len, envp);
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
    
            if(piped == 0){
                    if (strcmp(RequestMT,"&") == 0){
                                               printf("here.....");
                        
                        arg_struct args_pass2 = {
                            .command_index = command_index,
                            .args = args,
                            .pathlist = pathlist,
                            .args_len = args_len,
                            .envp = envp
                        };

                        pthread_create(&thread, NULL, run_command, (void*) &args_pass2);
                    } else run_command(command_index, args, pathlist, args_len, envp);
            }
            }
        }
    
    pthread_join(thread, NULL);
    mailFreeAll(watchmail);
    userFreeAll(watchuser);
    
    if(watching_users == 1){
        pthread_cancel(watchuser_tid);
        pthread_join(watchuser_tid, NULL);
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

// Creates thread to watch the mail and count
void *watchmail_thread(void *arg){
    
    char* filepath = (char*)arg;
    struct stat stat_path;
    
    stat(filepath, &stat_path);
    long old_size = (long)stat_path.st_size;
    
    time_t curtime;
    while(1){
        time(&curtime);
        stat(filepath, &stat_path);
        if((long)stat_path.st_size != old_size){
            printf("\a\nBEEP! You got mail in %s at time %s\n", filepath, ctime(&curtime));
            fflush(stdout);
            old_size = (long)stat_path.st_size;
        }
        sleep(1);
        
    }
    
    
}

// Creates thread to watch the user
void *watchuser_thread(void *arg){
    struct utmpx *up;
    while(1){
        setutxent();
        while(up = getutxent()){
            if ( up->ut_type == USER_PROCESS )
            {
                pthread_mutex_lock(&mutex_user);
                struct User_struct* user = findUser(watchuser, up->ut_user);
                if(user != NULL){
                    if(user->logged_on == 0){
                        printf("\n%s has logged on %s from %s\n", up->ut_user, up->ut_line, up ->ut_host);
                        user->logged_on = 1;
                    }
                }
                pthread_mutex_unlock(&mutex_user);
            }
        }
        sleep(1);
    }
}

// Decide which command to use depending on the command index
void run_command(int command_index, char** args, char* pathlist, int args_len, char** envp){
    pthread_mutex_lock(&mutex);
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
                        printf("Beginning...\n");
                        char *result = which(args[i], pathlist);
                        if (result != NULL) {
                            printf("%s\n", result);
                            free(result);
                        } else {
                            printf("%s not found\n", args[i]);
                        }
                        printf("End...\n");
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
        case 11: // noclobber
            if(noclobber == 0){
                printf("Noclobber on!\n");
                noclobber = 1;
            }else{
                printf("Noclobber off!\n");
                noclobber = 0;
            }
            break;
        case 12: // watchuser
            if(args_len == 2){
                printf("Watching user %s\n", args[1]);
                
                char* user = (char *)malloc(strlen(args[1]));
                strcpy(user, args[1]);
                
                pthread_mutex_lock(&mutex_user);
                watchuser = userAppend(watchuser, user);
               pthread_mutex_unlock(&mutex_user);
                
                
                printf("HEREEEEEE:D");
                if(watching_users == 0){
                    pthread_create(&watchuser_tid, NULL, watchuser_thread , NULL);
                    watching_users = 1;
                }
            }else{
                printf("watchuser: Not enough arguments\n");
            }
            
            break;
        case 13: // watchmail
            printf("Watching mail\n");
            
            if(args_len == 2){
                struct stat buffer;
                int exist = stat(args[1],&buffer);
                if(exist == 0){
                    pthread_t thread_id;
                    
                    char* filepath = (char *)malloc(strlen(args[1]));
                    strcpy(filepath, args[1]);
                    pthread_create(&thread_id, NULL, watchmail_thread, (void *)filepath);
                    watchmail = mailAppend(watchmail, filepath, thread_id);
                }else{
                    printf("watchmail: %s does not exist\n", args[1]);
                }
                
            }else if(args_len == 3){
                if(strcmp(args[2], "off") == 0){
                    watchmail = mailListRemoveNode(watchmail,args[1]);
                }else{
                    printf("watchmail: Wrong third argument\n");
                }
            }else{
                printf("watchmail: Invalid amount of arguments\n");
            }
            
            break;
        default:
             //printf("Command [%s] does not exist!\n", command);
            run_external(args, pathlist, args_len, envp);
    }
    
            pthread_mutex_unlock(&mutex);
}

// Runs an external command
void run_external(char** args, char* pathlist, int args_len, char** envp){
    char *cmd_path;
    
    if (args[0][0] == '.' || args[0][0] == '/') {
        cmd_path = (char *) malloc(strlen(args[0]));
        strcpy(cmd_path, args[0]);
    } else {
        cmd_path = which(args[0], pathlist);
    }

    int access_result = access(cmd_path, F_OK | X_OK);
    struct stat path_stat;
    stat(cmd_path, &path_stat);
    int has_background = strcmp(args[args_len-1], "&");
    
    if(has_background == 0){
        args[args_len-1] = NULL;
    }
    if (access_result == 0 && S_ISREG(path_stat.st_mode)) {
        if (cmd_path != NULL) {
            pid_t child_pid = fork();
            
            
            if (child_pid == 0) {
                int redirect_index = -1;
                for(int i = 0; i < args_len; i++){
                    if(strcmp(args[i], ">") == 0
                       || strcmp(args[i], ">&") == 0
                       || strcmp(args[i], ">>") == 0
                       || strcmp(args[i], ">>&") == 0
                       || strcmp(args[i], "<") == 0){
                        redirect_index = i;
                    }
                }
                
                
                int execute = 1;
                
                if(redirect_index != -1){
                    char* file_dest = args[redirect_index+1];
                    char* redirect_string = args[redirect_index];
                    
                    int permissions = 0666;
                    int exists = 0;
                    
                    struct stat st;
                    
                    if(stat(file_dest, &st) == 0){
                        exists = 1;
                    }

                    if(strcmp(redirect_string, ">") == 0){
                        if(noclobber == 1 && exists == 1){
                            printf("Noclobber on! Cannot overwrite %s\n", file_dest);
                            execute = 0;
                        }else{
                            int fid = open(file_dest, O_WRONLY|O_CREAT|O_TRUNC, permissions);
                            close(STDOUT_FILENO);
                            dup(fid);
                            close(fid);
                        }
                    }else if(strcmp(redirect_string, ">&") == 0){
                        if(noclobber == 1 && exists == 1){
                            printf("Noclobber on! Cannot overwrite %s\n", file_dest);
                            execute = 0;
                        }else{
                            int fid = open(file_dest, O_WRONLY|O_CREAT|O_TRUNC, permissions);
                            close(STDOUT_FILENO);
                            dup(fid);
                            close(STDERR_FILENO);
                            dup(fid);
                            close(fid);
                        }
                    }else if(strcmp(redirect_string, ">>") == 0){
                        if(noclobber == 1 && exists == 0){
                            printf("Noclobber on! File %s does not exist\n", file_dest);
                            execute = 0;
                        }else{
                            int fid = open(file_dest, O_WRONLY|O_CREAT|O_APPEND, permissions);
                            close(STDOUT_FILENO);
                            dup(fid);
                            close(fid);
                        }
                    }else if(strcmp(redirect_string, ">>&") == 0){
                        if(noclobber == 1 && exists == 0){
                            printf("Noclobber on! File %s does not exist\n", file_dest);
                            execute = 0;
                        }else{
                            int fid = open(file_dest, O_WRONLY|O_CREAT|O_APPEND, permissions);
                            close(STDOUT_FILENO);
                            dup(fid);
                            close(STDERR_FILENO);
                            dup(fid);
                            close(fid);
                        }
                    }else if(strcmp(redirect_string, "<") == 0){
                        if(stat(file_dest, &st) == -1){
                            printf("Error opening %s\n", file_dest);
                            execute = 0;
                        }else{
                            int fid = open(file_dest, O_RDONLY);
                            close(STDIN_FILENO);
                            dup(fid);
                            close(fid);
                        }
                    }
                    
                    for(int i = redirect_index; i < args_len; i++){
                        args[i] = NULL;
                    }
                }
                
                if(execute == 1){
                    int ret = execve(cmd_path, args, envp);
                }else{
                    exit(0);
                }
            }
            
            int child_status;
            
            if(has_background == 0){
                waitpid(child_pid, &child_status, WNOHANG);
            } else {
                waitpid(child_pid, &child_status, 0);
            }
            
            
        } else {
            printf("%s: Command not found\n", args[0]);
        }
    } else {
        printf("%s\n", "Invalid Command");
        printf("Access Error: %i\n", errno);
    }
    
    free(cmd_path);
}

// Function to handle the printenv command
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

// Function to handle which search
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

// Function to handle the where command, searches file in path
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


// Function to list
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

// Watch User Code...

struct User_struct* userAppend(struct User_struct* head, char* user){
    struct User_struct* current = head;
    
    
    if(head != NULL){
        while(current->next != NULL){
            current = current->next;
        }
    }
    
    struct User_struct* new = (struct User_struct*)malloc(sizeof(struct User_struct));
    
    new->user = user;
    new->logged_on = 0;
    
    new->next = NULL;
    strcpy(new->user, user);
    
    if(head != NULL){
        current->next = new;
    }else{
        head = new;
    }
    
    return head;
}

struct User_struct* findUser(struct User_struct* head, char* user){
    struct User_struct* current = head;
    
    while(current != NULL){
        if(strcmp(user, current->user) == 0){
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

struct User_struct* userListRemoveNode(struct User_struct* head, char* user){
    struct User_struct* current = head;
    
    //At head of list
    if(strcmp(current->user, user) == 0){
        head = current->next;
        freeUser_struct(current);
        return head;
    }
    
    while(current!=NULL){
        struct User_struct* next = current->next;
        
        if(next != NULL){
            if(strcmp(next->user, user) == 0){
                current->next = next->next;
                freeUser_struct(next);
            }
        }
        
        current = next;
    }
    
    return head;
}

void freeUser_struct(struct User_struct* node){
    free(node->user);
    free(node);
}

void userFreeAll(struct User_struct* head){
    struct User_struct* current = head;
    while(current != NULL){
        struct User_struct* toDelete = current;
        current = current->next;
        free(toDelete->user);
        free(toDelete);
    }
}

// End of Watch User

// Watchmail

struct Mail_struct* mailAppend(struct Mail_struct* head, char* filepath, pthread_t id){
    struct Mail_struct* current = head;
    
    
    if(head != NULL){
        while(current->next != NULL){
            current = current->next;
        }
    }
    
    struct Mail_struct* new = (struct Mail_struct*)malloc(sizeof(struct Mail_struct));
    
    new->filepath = filepath;
    
    new->next = NULL;
    strcpy(new->filepath, filepath);
    new->thread_id = id;
    
    if(head != NULL){
        current->next = new;
    }else{
        head = new;
    }
    
    return head;
}

// Traverse the Mail Struct Node
void mailTraverse(struct Mail_struct* head){
    struct Mail_struct* current = head;
    
    while(current->next != NULL){
        printf("File: %s Id: %i\n", current->filepath, current->thread_id);
        current = current->next;
    }
}

// Remove a Mail Struct Node from the List
struct Mail_struct* mailListRemoveNode(struct Mail_struct* head, char* filepath){
    struct Mail_struct* current = head;
    
    //At head of list
    if(strcmp(current->filepath, filepath) == 0){
        head = current->next;
        freeMail_struct(current);
        return head;
    }
    
    while(current!=NULL){
        struct Mail_struct* next = current->next;
        
        if(next != NULL){
            if(strcmp(next->filepath, filepath) == 0){
                current->next = next->next;
                freeMail_struct(next);
            }
        }
        
        current = next;
    }
    
    return head;
}

void freeMail_struct(struct Mail_struct* node){
    printf("%i, %s\n",node->thread_id,node->filepath);
    pthread_cancel(node->thread_id);
    pthread_join(node->thread_id, NULL);
    free(node->filepath);
    free(node);
}




void mailFreeAll(struct Mail_struct* head){
    struct Mail_struct* current = head;
    while(current != NULL){
        struct Mail_struct* toDelete = current;
        current = current->next;
        pthread_cancel(toDelete->thread_id);
        pthread_join(toDelete->thread_id, NULL);
        free(toDelete->filepath);
        free(toDelete);
    }
}



// End of Watchmail

