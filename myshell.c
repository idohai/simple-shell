#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>

// force process to ignore the SIGINT signal.
// return 0 on success, exit(1) on failure.
int ignore_sigint(){ 
    struct sigaction sa;
    sa.sa_sigaction = (void*)SIG_IGN;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, 0)== -1){
        perror("sigint handler failed");
        exit(1);
    }
    return 0;
}

// activate SIGINT signal after it was ignored using ignore_sigint().
// return 0 on success, exit(1) on failure.
int activate_sigint(){
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, 0)== -1){
        perror("sigint handler failed");
        exit(1);
    }
    return 0;
}

// force process to ignore the SIGCHLD signal.
// used for preveting zombies
// return 0 on success, exit(1) on failure.
int ignore_sigchld(){
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = SA_NOCLDSTOP; // signal only on child death
    if (sigaction(SIGCHLD, &sa, 0) == -1){
        perror("sigchld handler failed");
        exit(1);
    }
    return 0;
}

// find symbol in arglist. ASSUMPTION: symbol does not appear in the beginning of arglist
// return value: symbol index (loc) in arglist. 0 if symbol not in arglist
int find_symbol(int count, char **arglist, char *symbol){
    for (int i=0; i<count; i++){
        if (!strcmp (arglist[i], symbol)){
            return i;
        }
    }
    return 0;
}
// execute command from arglist in background
// return value: 1 on success, 0 on failure.
int ampersand (int count, char **arglist){
    int pid;
    if ((pid=fork()) < 0){
        perror("fork creation failed");
        return 0;
    }
    if (pid == 0){
        arglist[count-1] = NULL;
        execvp(arglist[0], arglist);
        perror("execvp failed");
        exit(1);
    }
return 1;
}
// pipe two commands from arglist (seperated by "|")
// return value: 1 on success, 0 on failure.
int piping (int pipe_symbol_location, char **arglist){
    int pid1, pid2;
    int fd[2];
    arglist[pipe_symbol_location] = NULL;
    if (pipe(fd)<0){
        perror("An error occured with opening the pipe");
        return 0;
    }
    pid1 = fork();
    if (pid1 < 0){
        perror("fork creation failed");
        return 0;
    }
    else if(pid1 == 0){
        activate_sigint();
        if (dup2(fd[1], STDOUT_FILENO)==-1){
            perror("dup2 failed");
            exit(1);
        }
        if (close (fd[0] == -1 )){
            perror ("closing fd[0] failed");
            exit(1);
        }
        if (close (fd[1])){
            perror("closing fd[1] failed");
            exit(1);
        }
        execvp(arglist[0], arglist);
        perror("execvp failed");
        exit(1);
    }
    pid2 = fork();
    if (pid2 < 0){
        perror ("fork creation failed");
        return 0;
    }
    if (pid2==0){
        activate_sigint();
        if (dup2(fd[0], STDIN_FILENO) == -1){
            perror("dup2 failed");
            exit(1);
        }
        if (close(fd[0]) == -1){
            perror ("closing fd[0] failed");
            exit(1);
        }
        if (close(fd[1])==-1){
            perror ("closing fd[1] failed");
            exit(1);
        }
        execvp(arglist[pipe_symbol_location+1], &arglist[pipe_symbol_location+1]);
        perror("execvp failed");
        exit(1);
    }
    if (close(fd[0]) == -1){
        perror ("closing fd[0] failed");
        return 0;
    }
    if (close(fd[1])==-1){
        perror ("closing fd[1] failed");
        return 0;
    }
    waitpid(pid1, NULL, WUNTRACED);
    waitpid(pid2, NULL, WUNTRACED);
    return 1;
} 
// redirecting command output to file. arglist is "command > file"
// return value: 1 on success, 0 on failure.
int output_redirection(int output_redirection_location, char **arglist){
    int pid;
    arglist[output_redirection_location] = NULL;
    if ((pid = fork()) < 0){
        perror("fork creation failed");
        return 0;
    }
    else if (pid == 0){
        activate_sigint();
        int fd = open(arglist[output_redirection_location+1], O_WRONLY|O_CREAT, 0666);
        if (fd<0){
            perror("file open failed");
        }
        if (dup2(fd, STDOUT_FILENO)<0){
            perror("Couldn't redirect output");
        }
        close(fd);
        execvp(arglist[0], arglist);
        perror("execvp failed");
        exit(1);
        
    }
    return 1;
}
// prepare shell execution by setting paret process signal handlers
// return 0 on success and 1 on failure BUT will perform exit(1) in case of sigaction failure.
int prepare(void){
    if (ignore_sigint()){
        return 1; // sigint handler failed, error message was printed.
    }
    if (ignore_sigchld()){
        return 1;
    }
    return 0; // return 0 on success, any other value if failure
}

int process_arglist(int count, char **arglist){

    int pipe_symbol_location, output_redirection_location;
    if (!strcmp (arglist[count-1], "&")){ // if background process
        return ampersand(count, arglist);
    }
    else if ((pipe_symbol_location = find_symbol(count, arglist, "|"))){ //if piping operation
        return piping(pipe_symbol_location, arglist);    }
    else if ((output_redirection_location = find_symbol(count, arglist, ">"))){ //if output redirection
        return output_redirection(output_redirection_location, arglist);
    }
    else{ // else - foreground process
        int pid;
        if ((pid=fork()) < 0){
            perror("fork creation failed");
            return 0;
        }
        else if (pid == 0){
            if (activate_sigint()){
                return 0;
            }
            execvp(arglist[0], arglist);
            perror("execvp failed");
            exit(1);
        }
        else {
            waitpid(pid, NULL, WUNTRACED);
        }
        return 1;
    }
    return 1;
}

int finalize(void){
    return 0; //parent process killed with his signal handlers. finalize not needed.
}

// COMPILE:
// gcc -O3 -D_POSIX_C_SOURCE=200809 -Wall -std=c11 shell.c myshell.c