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