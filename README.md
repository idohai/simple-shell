# simple-shell
Simple shell implemeneted with C.  

Functionality:  
* executing commands,  
* background execution ('&'),  
* commands single piping ('|'),  
* output redirection ('>')  

use ctrl+D to kill the simple-shell

Compile command: gcc -O3 -D_POSIX_C_SOURCE=200809 -Wall -std=c11 shell.c myshell.c

#prcoess_management #pipes #signals #syscalls
