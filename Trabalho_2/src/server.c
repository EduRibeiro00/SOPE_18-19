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
#include "server.h"

int main() {

    char salt[SALT_LEN + 1];

    generateRandomSalt(salt);

    printf("%s\n", salt);
    return 0;
}

// ---------------------------------

int createThreads(pthread_t* tids, int numThreads) {

    for(int i = 0; i < numThreads; i++) {
        // pthread_create(&tids[i], NULL, func, arg);
    }
}

// ---------------------------------

bool checkPassword(char* password) {

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

        // VAI BUSCAR SALT E HASH
        char* salt = "a12011bdfabf719"; // salt de exemplo
        char* hash = "7d62e03ec1bedef3d0a8cdd0ba29716e3568025c5b7a3acd51e0fb11cbe306be";

        // concatenates password and salt
        char value[500];
        value[0] = '\0';
        strcat(value, password);
        strcat(value, salt);
        int len = strlen(value) + 1;

        // sends it to coprocess
        if(write(fd1[WRITE], value, len) != len) {
            perror("Write to pipe");
            exit(EXIT_FAILURE);
        }

        // reads the generated hash from the coprocess
        if((len = read(fd2[READ], value, 500)) < 0) {
            perror("Read from pipe");
            exit(EXIT_FAILURE);
        }

        // if child closed pipe
        if(len == 0) {
            fprintf(stderr, "Child closed pipe!");
            return false;
        }

        value[len] = '\0';

        return (!strcmp(value, hash));
    }
    else { // child

    }
}