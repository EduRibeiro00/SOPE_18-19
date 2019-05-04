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
#include "user.h"

int main(int argc, char* argv[]) {

    if(argc != 6) {
        fprintf(stderr, "Illegal use of arguments! Usage: %s <ID> <\"password\"> <op_delay> <op_code> <args> \n", argv[0]);
    }

    pid_t pid = getpid();

    char fifoName[500];
    sprintf(fifoName, "%s%d", USER_FIFO_PATH_PREFIX, pid);

    // create user FIFO
    if(mkfifo(fifoName, 0660) < 0) {
        if(errno == EEXIST) 
            fprintf(stderr, "FIFO '%s' already exists\n", fifoName);
        else {
            fprintf(stderr, "Can't create server FIFO!");
            exit(EXIT_FAILURE);
        }
    }

    req_value_t msgValue;
    req_header_t msgHeader;

    // fill in the message header
    msgHeader.pid = pid;
    msgHeader.account_id = argv[1];
    strcpy(msgHeader.password, argv[2]);
    msgHeader.op_delay_ms = argv[3];

    msgValue.header = msgHeader;

    int opCode = atoi(argv[4]);


    if(opCode == OP_CREATE_ACCOUNT) { // if create operation

        char args[500];
        argv[5]++;
        int len = strlen(argv[5]) - 1; 
        strncpy(args, argv[5], len);
        args[len] = '\0';
        


        printf("%s\n", args);

        req_create_account_t msgArgs;
        msgArgs.account_id
    }
    else if(opCode == OP_TRANSFER) { // if transfer operation


    }


    return 0;
}