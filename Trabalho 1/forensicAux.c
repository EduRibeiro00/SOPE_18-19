#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

void commandToString(char* result, int max_size, char* command, char* file){

        int fd;

        pid_t pid = fork(); //creates new child process
        if(pid == -1){
            perror("fork");
            exit(3);
        }
        else if(pid > 0){ //parent process

            int status;
            pid_t childPid = wait(&status); //waits for child
            if(childPid == -1 || WEXITSTATUS(status) != 0){
                printf("Problem with wait/child!\n");
                exit(3);
            }
        }
        else if(pid == 0){ //child process

            //creates a new file, in order to store the output of the command call
            if((fd = open("123.txt", O_RDWR | O_CREAT, 0644)) == -1){
                perror("Temporary file");
                exit(3);
            }

            if(dup2(fd, STDOUT_FILENO) == -1){ //the standard output now goes to the file previously created
                perror("dup2");
                exit(3);
            }

            execlp(command, command, file, NULL); //executes command
            printf("Error with exec!");
            exit(3);
        }


        if((fd = open("123.txt", O_RDWR, 0644)) == -1){ //opens the file (this time in the parent process)
            perror("Temporary file");
            exit(3);
        }

        int rb;
        if((rb = read(fd, result, max_size)) <= 0){
            perror("Error with read");
            exit(3);
        }

        result[rb - 1] = 0; //gets rid of the newline

        if(close(fd) == -1){ //closes the file descriptor
            perror("close");
            exit(3);
        }

        if(unlink("123.txt") == -1){ //deletes the temporary file
            perror("unlink");
            exit(3);
        }

}