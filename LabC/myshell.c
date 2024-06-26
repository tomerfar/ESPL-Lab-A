#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h> // For getcwd
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "LineParser.h"
#include <signal.h>
#include <fcntl.h>
#include <linux/stat.h>
#include <linux/limits.h> // For PATH_MAX

//#define PATH_MAX 4096
#define MY_MAX_INPUT 2048
#define TERMINATED  -1
#define RUNNING 1
#define SUSPENDED 0
#define HISTLEN 20
#define MAX_BUF 200

 char input[MY_MAX_INPUT];

 typedef struct process{
        cmdLine* cmd;              /* the parsed command line*/
        pid_t pid; 		           /* the process id that is running the command*/
        int status;                /* status of the process: RUNNING/SUSPENDED/TERMINATED */
        struct process *next;	   /* next process in chain */
} process;

process* process_list = NULL; // Initializing a global processes linked list
char history[HISTLEN][MAX_BUF]; // The command history array
int history_count = 0;          // Number of commands in history
int history_start = 0;          // Index of the oldest command
int history_end = 0;            // Index of the newest command

void add_history(const char *cmd) {
    strncpy(history[history_end], cmd, MAX_BUF);
    history[history_end][MAX_BUF - 1] = '\0'; // Ensure null-termination
    history_end = (history_end + 1) % HISTLEN;

    if (history_count < HISTLEN) {
        history_count++;
    } else {
        history_start = (history_start + 1) % HISTLEN; // Overwrite oldest command
    }
}

void print_history() {
    for (int i = 0; i < history_count; i++) {
        int index = (history_start + i) % HISTLEN;
        printf("%d %s\n", i + 1, history[index]);
    }
}

const char* get_history(int index) {
    if (index < 1 || index > history_count) {
        fprintf(stderr, "Error: Invalid history index.\n");
        return NULL;
    }
    return history[(history_start + index - 1) % HISTLEN];
}




void addProcess(process** process_list, cmdLine* cmd, pid_t pid){
    process *new_head = (process *)malloc(sizeof(process));
    if(new_head == NULL){
        fprintf(stderr, "Failed to allocate memory for new process.\n");
    }
    new_head->cmd = cmd;
    new_head->pid = pid;
    new_head->status = RUNNING;
    new_head->next = *process_list;
    *process_list = new_head;   
}


void updateProcessStatus(process* process_list, pid_t pid, int status) {
    process* current = process_list;
    while (current != NULL) {
        if (current->pid == pid) {
            current->status = status;
            break;
        }
        current = current->next;
    }
}


void freeProcess(process *processToFree){

    free(processToFree);
}


process *removeProcess(process *prev, process *current) {
    process *ret = NULL;
    if (prev == NULL) {
        if (current->next == NULL) {
            process_list = NULL;
            ret = NULL;
        } else {
            process_list = current->next;
            ret = current->next;
        }
        freeCmdLines(current->cmd); // Free cmdLine structure
        free(current);
    } else {
        ret = current->next;
        prev->next = current->next;
        freeCmdLines(current->cmd); // Free cmdLine structure
        free(current);
    }
    return ret;
}


void updateProcessList(process **process_list)
{
    process *current = *process_list;
    while (current)
    {
        int currentStatus = 0; // Initialize currentStatus to a default value
        int result = waitpid(current->pid, &currentStatus, WNOHANG | WUNTRACED);
        if (result == 0)
        {
            currentStatus = RUNNING;
        }
        else
        {
            if (WIFSTOPPED(currentStatus))
            {
                currentStatus = SUSPENDED;
            }
            else if (WIFCONTINUED(currentStatus))
            {
                currentStatus = RUNNING;
            }
            else if (WIFEXITED(currentStatus) || WIFSIGNALED(currentStatus))
            {
                currentStatus = TERMINATED;
            }
        }

        if (currentStatus != current->status)
        {
            updateProcessStatus(*process_list, current->pid, currentStatus);
        }
        current = current->next;
    }
}



void printProcessList(process** process_list){
    process *prev = NULL;
    process *current = *process_list;
    int i = 0;
        printf("%-8s %-8s %-16s %-8s\n", "Index", "PID", "Command", "STATUS");
        while(current != NULL){
        printf("%-8d %-8d %-16s ", i, current->pid, current->cmd->arguments[0]);
        if(current->status == RUNNING){
            printf("Running\n");
            prev = current;
            current = current->next;
        }
        else if (current->status == TERMINATED){
            printf("Terminated\n");
            current = removeProcess(prev, current);
        }
        else if(current->status == SUSPENDED){
            printf("Suspended\n");
            current = current->next;
        }
        i += 1;
        } 
    }


void Procsfunc(process **process_list)
{
    updateProcessList(process_list);
    printProcessList(process_list);
}


void freeProcessList(process **process_list) {
    process *current = *process_list;
    while (current != NULL) {
        process *next = current->next;
        freeCmdLines(current->cmd);
        free(current);
        current = next;
    }
    *process_list = NULL; // Set the process_list to NULL after freeing all processes
}



void handleAlarm(int process_id){
    
    if(kill(process_id, SIGCONT) == -1){
        fprintf(stderr, "failed to send SIGCONT sign\n");
    }
    else{
        printf("SIGCONT sign was sent to process: %d\n", process_id);
    }
}

 void handleBlast(int process_id){
    if(kill(process_id, SIGKILL) == -1){
        fprintf(stderr, "failed to send SIGKLILL sign\n");
    }
    else{
        printf("SIGKILL sign was sent to process: %d\n", process_id);
    }

}

 void handleSleep(int process_id){
     if(kill(process_id, SIGTSTP) == -1){
         fprintf(stderr, "failed to send SIGKTSTP sign\n");
     }
     else{
         printf("SIGTSTP sign was sent to process: %d\n", process_id);
     }
 }

void execute(cmdLine *pCmdLines, int debug)
{
   pid_t child_pid; /* child process id*/
   if((child_pid = fork()) == -1){ //couldn't fork
    perror("fork() error");
    exit(1);
   }
   else if(child_pid == 0){ // enters child process
    if(pCmdLines->inputRedirect != NULL){
        int fd = open(pCmdLines->inputRedirect, O_RDONLY);
        if(fd == -1){
            perror("open() error");
            _exit(1);
        }
            if(dup2(fd,STDIN_FILENO) == -1){
            perror("dup2() error");
            _exit(1);
        }
            close(fd);
    }
    if(pCmdLines->outputRedirect != NULL){
        
        int fd = open(pCmdLines->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC ,S_IRUSR | S_IWUSR);
        if(fd == -1){
            perror("open() error");
            _exit(1);
        }
        if(dup2(fd,STDOUT_FILENO) == -1){
            perror("dup2() error");
            _exit(1);
        }
        close(fd);
    }
    

    if(execvp(pCmdLines->arguments[0], pCmdLines->arguments) == -1){ /* execv needs to recieve the full path name, while execvp needs only to recieve the filename, and then it searches this name inside the cd*/
      process_list->status = TERMINATED;
      perror("execvp() error");
      _exit(1);  
    }
   }
   else{ //creates a child process that runs conccurently with the parent process.
        addProcess(&process_list, parseCmdLines(input), child_pid);
        if(debug){
            fprintf(stderr, "PID: %d\n", child_pid);
            fprintf(stderr,"Executing Command:%s\n", pCmdLines->arguments[0]);
        }

        if(pCmdLines->blocking){ //blocking is 1 
            waitpid(child_pid, NULL, 0);
            updateProcessStatus(process_list, child_pid, TERMINATED);
        }
    }
}

void executePipe(cmdLine *pCmdLines, int debug){
    int pipefd[2];
    pid_t cpid1, cpid2;
    int saved_stdin = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);

    if(pCmdLines -> outputRedirect != NULL){
        fprintf(stderr, "Error: Output redirection '>' not allowed for this command.\n");
        _exit(1);
    }
    if(pCmdLines->next -> inputRedirect != NULL){
        fprintf(stderr, "Error: Input redirection '<' not allowed for this command.\n");
        _exit(1);
    }
     if (pipe(pipefd) == -1) {
        perror("pipe error");
        exit(EXIT_FAILURE);
    }

    if(pCmdLines -> inputRedirect != NULL){
        int fd = open(pCmdLines->inputRedirect, O_RDONLY);
        if(fd == -1){
            perror("open() error");
            _exit(1);
        }

            if(dup2(fd,STDIN_FILENO) == -1){
            perror("dup2() error");
            _exit(1);
        }
            close(fd);    
    }
    cpid1 = fork();
    if (cpid1 == -1) {
        perror("Error: couldn't fork");
        exit(EXIT_FAILURE);
    }

    if(cpid1 == 0){ //enters the child process

        close(STDOUT_FILENO);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        if(execvp(pCmdLines->arguments[0], pCmdLines->arguments) == -1){ /* execv needs to recieve the full path name, while execvp needs only to recieve the filename, and then it searches this name inside the cd*/
            process_list->status = TERMINATED;
            perror("execvp() error inside cpid1");
            _exit(1);  
        }
        if(debug){
            fprintf(stderr, "PID: %d\n", cpid1);
            fprintf(stderr,"Executing Command:%s\n", pCmdLines->arguments[0]);
        }
    }

    close(pipefd[1]);

    if(pCmdLines->next -> outputRedirect != NULL){
         int fd = open(pCmdLines->next ->outputRedirect, O_WRONLY);
        if(fd == -1){
            perror("open() error");
            _exit(1);
        }
        if(dup2(fd,STDOUT_FILENO) == -1){
            perror("dup2() error");
            _exit(1);
        }
        close(fd);
    }

    cpid2 = fork();
    if (cpid2 == -1) {
        perror("Error: couldn't fork");
        exit(EXIT_FAILURE);
    }
    
     if (cpid2 == 0){ //enters a child process

        close(STDIN_FILENO);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        if(execvp(pCmdLines->next->arguments[0], pCmdLines->next->arguments) == -1){ /* execv needs to recieve the full path name, while execvp needs only to recieve the filename, and then it searches this name inside the cd*/
            process_list->status = TERMINATED;
            perror("execvp() error inside cpid2");
            _exit(1);  
        }
         if(debug){
            fprintf(stderr, "PID: %d\n", cpid2);
            fprintf(stderr,"Executing Command:%s\n", pCmdLines->next->arguments[0]);
        }
    }
    addProcess(&process_list, parseCmdLines(input), cpid1);
    addProcess(&process_list, parseCmdLines(input), cpid2);

    close(pipefd[0]);
    waitpid(cpid1, NULL, 0);
    updateProcessStatus(process_list, cpid1, TERMINATED);
    waitpid(cpid2, NULL, 0);
    updateProcessStatus(process_list, cpid2, TERMINATED);
    dup2(saved_stdin, STDIN_FILENO);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdin);
    close(saved_stdout);
}


int main(int argc, char **argv)
{
    char cwd[PATH_MAX];
    cmdLine* parseCmd;
    

    int debug = 0;
    for(int i = 0; i < argc; i++){
        //updating debug mode if we passed "-d"
        if(strcmp(argv[i], "-d") == 0){ //the argument that we passed is -d
            debug = 1;
        }
    }

    if(getcwd(cwd, sizeof(cwd)) != NULL){
            printf("%s\n ", cwd); //check in the frontal lab may need to go inside the while loop
        }
        else{
            perror("getcwd() error");
            return 1;
        }
     
    while(1){ /* infinite loop*/

        if(fgets(input, sizeof(input), stdin) == NULL){
            perror("Errors in reading the input");
            return 1;
        }

        
        if (strcmp(input, "quit\n") == 0) {
        freeProcessList(&process_list);    
        printf("Exiting shell.\n");
        break;
        }
        

        if (strcmp(input, "!!\n") == 0) {
            const char *last_cmd = get_history(history_count);
            if (last_cmd != NULL) {
                strcpy(input, last_cmd);
                printf("Executing: %s\n", input);
            } else {
                continue;
            } 
        } else if (input[0] == '!' && isdigit(input[1])) {
            int index = atoi(&input[1]);
            const char *cmd = get_history(index);
            if (cmd != NULL) {
                strcpy(input, cmd);
                printf("Executing: %s\n", input);
            } else {
                continue;
            }
        }

        add_history(input); //adding the unparsed command to the commands queue

        parseCmd = parseCmdLines(input);
        if(parseCmd == NULL){
            perror("Error in parsing command");
            continue;
        }


        /* checks the type of command the shell recieved and reacts accordingly */
        if(strcmp(parseCmd->arguments[0], "cd") == 0){ //change directory
            //printf("argCount: %d\n", parseCmd->argCount);
            if(parseCmd->argCount < 2){
                fprintf(stderr, "Error: file or directory name is missing\n");
            }
            else if(chdir(parseCmd->arguments[1]) == -1){ /* checks if the next argument is a name of an actual file */
                fprintf(stderr, "cd: %s: No such file or directory \n", parseCmd->arguments[1]);
            }
            else {
        
                if (getcwd(cwd, sizeof(cwd)) != NULL) {
                    printf("New Directory: %s\n", cwd); // Print the new current directory
                } else {
                    perror("getcwd() error");
                }
            
            freeCmdLines(parseCmd);
            continue; /* cd command is handled internally by the shell and doesn't require creating a new process or executing any other command*/
                }
        }
        else if(strcmp(parseCmd->arguments[0], "alarm") == 0){ //wakes up a sleeping process
            if(parseCmd->argCount < 2){
                fprintf(stderr, "alarm: pid is missing\n");
            }
            else{
                int pid = atoi(parseCmd->arguments[1]);
                handleAlarm(pid);
            }
            freeCmdLines(parseCmd);
            continue;
        }
        else if(strcmp(parseCmd->arguments[0], "blast") == 0){ //terminates a running/sleeping process
            if(parseCmd->argCount < 2){
                fprintf(stderr, "blast: pid is missing\n");
            }
            else{
                int pid = atoi(parseCmd->arguments[1]);
                handleBlast(pid);
            }
            freeCmdLines(parseCmd);
            continue;
        }
        else if(strcmp(parseCmd->arguments[0], "sleep") == 0){
            if(parseCmd->argCount < 2){
                fprintf(stderr, "sleep: pid is missing\n");
            }
            else{
                int pid = atoi(parseCmd->arguments[1]);
                handleSleep(pid);
            }
            freeCmdLines(parseCmd);
            continue;
        }
        else if(strcmp(parseCmd->arguments[0], "procs") == 0){ //printing all of the child processes in the program
            Procsfunc(&process_list);
            freeCmdLines(parseCmd);
            continue;
        }
        else if(strcmp(parseCmd->arguments[0], "history") == 0){
            print_history();
            freeCmdLines(parseCmd);
            continue;
        }
        else if(parseCmd->next != NULL){
            executePipe(parseCmd, debug);
        }

        else if(parseCmd->next == NULL)
        {
            execute(parseCmd, debug);
        }
        //if the commands is different than all of the above, it handles it anyway with execute or executePipe
        freeCmdLines(parseCmd);
    }

    return 0;
}
