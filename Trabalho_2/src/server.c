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

        close(fd1)   
    }
}