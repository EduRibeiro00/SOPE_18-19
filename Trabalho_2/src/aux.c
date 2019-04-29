#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/time.h> 
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#include "constants.h"
#include "sope.h"
#include "types.h"
#include "aux.h"

// string tem de ter SALT_LEN + 1 length
void generateRandomSalt(char* salt) {

    for(int i = 0; i < SALT_LEN; i++) {
        int random = rand() % 2;
        int value;

        if(random)
            value = rand() % 10 + 48;
        else
            value = rand() % 6 + 97;

        salt[i] = (char) value;
    }

    salt[SALT_LEN] = '\0';
}

// ---------------------------------

char* generateHash(char* password, char* salt) {

     int fd1[2], fd2[2];
    pid_t pid;

    if(pipe(fd1) < 0 || pipe(fd2) < 0) {
        perror("Pipes for coprocess");
        exit(EXIT_FAILURE);
    }

    pid = fork();

    if(pid < 0) {
        perror("Fork for coprocess");
        exit(EXIT_FAILURE);
    }
    else if(pid > 0) { // parent

        close(fd1[READ]);
        close(fd2[WRITE]);

        // concatenates password and salt
        char value[500];
        value[0] = '\0';
        strcat(value, password);
        strcat(value, salt);
        int len = strlen(value);

        // sends it to coprocess
        if(write(fd1[WRITE], value, len) != len) {
            perror("Write to pipe");
            exit(EXIT_FAILURE);
        }

        close(fd1[WRITE]);

        // reads the generated hash from the coprocess
        if((len = read(fd2[READ], value, 500)) < 0) {
            perror("Read from pipe");
            exit(EXIT_FAILURE);
        }

        close(fd2[READ]);

        // if child closed pipe
        if(len == 0) {
            fprintf(stderr, "Child closed pipe!");
            return false;
        }

        value[len] = '\0';
        return strtok(value, " ");
    }
    else { // child

        close(fd1[WRITE]);
        close(fd2[READ]);

        // redirects stdin to pipe
        if(fd1[READ] != STDIN_FILENO) {
            if(dup2(fd1[READ], STDIN_FILENO) != STDIN_FILENO) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(fd1[READ]);
        }

        // redirects stdout to pipe
        if(fd2[WRITE] != STDOUT_FILENO) {
            if(dup2(fd2[WRITE], STDOUT_FILENO) != STDOUT_FILENO) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(fd2[WRITE]);
        }

        // calls sha256sum
        execlp("sha256sum", "sha256sum", NULL);
        perror("execlp");
        exit(EXIT_FAILURE);
    }
}