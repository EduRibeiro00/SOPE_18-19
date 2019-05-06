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

    // opens (and creates, if necessary) user log file
    // int userLogFile = open(USER_LOGFILE, O_WRONLY | O_CREAT, 0664);

    pid_t pid = getpid();

    char fifoName[500];
    sprintf(fifoName, "%s%d", USER_FIFO_PATH_PREFIX, pid);

    // create user FIFO
    if(mkfifo(fifoName, 0660) < 0) {
        if(errno == EEXIST) 
            fprintf(stderr, "FIFO '%s' already exists\n", fifoName);
        else {
            fprintf(stderr, "Can't create server FIFO!\n");
            exit(EXIT_FAILURE);
        }
    }

    req_value_t msgValue;
    req_header_t msgHeader;

    // fill in the message header
    msgHeader.pid = pid;
    msgHeader.account_id = atoi(argv[1]);
    strcpy(msgHeader.password, argv[2]);
    msgHeader.op_delay_ms = atoi(argv[3]);

    msgValue.header = msgHeader;

    int opCode = atoi(argv[4]);


    if(opCode == OP_CREATE_ACCOUNT) { // if create operation

        char args[500];
        strcpy(args, argv[5]);
        char* token;
        req_create_account_t msgArgs;

        token = strtok(args, " ");
        
        // account id
        if(token != NULL)
            msgArgs.account_id = atoi(token);
        else {
            fprintf(stderr, "Wrong arguments passed for this type of operation\n");
            exit(EXIT_FAILURE);
        }

        token = strtok(NULL, " ");    

        // account balance
        if(token != NULL)
            msgArgs.balance = atoi(token);
        else {
            fprintf(stderr, "Wrong arguments passed for this type of operation\n");
            exit(EXIT_FAILURE);
        }

        token = strtok(NULL, " ");    

        // account password
        if(token != NULL)
            strcpy(msgArgs.password, token);        
        else {
            fprintf(stderr, "Wrong arguments passed for this type of operation\n");
            exit(EXIT_FAILURE);
        }
    
        // to check if there is more arguments (which there shouldn't)
        token = strtok(NULL, " "); 
    
        if(token != NULL) {
            fprintf(stderr, "Wrong arguments passed for this type of operation\n");
            exit(EXIT_FAILURE);
        }

        msgValue.create = msgArgs;
    }
    else if(opCode == OP_TRANSFER) { // if transfer operation

        char args[500];
        strcpy(args, argv[5]);
        char* token;
        req_transfer_t msgArgs;

        token = strtok(args, " ");
        
        // account id
        if(token != NULL)
            msgArgs.account_id = atoi(token);
        else {
            fprintf(stderr, "Wrong arguments passed for this type of operation\n");
            exit(EXIT_FAILURE);
        }

        token = strtok(NULL, " ");    

        // money amount
        if(token != NULL)
            msgArgs.amount = atoi(token);
        else {
            fprintf(stderr, "Wrong arguments passed for this type of operation\n");
            exit(EXIT_FAILURE);
        }
    
        // to check if there is more arguments (which there shouldn't)
        token = strtok(NULL, " "); 
    
        if(token != NULL) {
            fprintf(stderr, "Wrong arguments passed for this type of operation\n");
            exit(EXIT_FAILURE);
        }

        msgValue.transfer = msgArgs;
    }
    else if(opCode == OP_BALANCE || opCode == OP_SHUTDOWN) {
        if(strcmp(argv[5], "") != 0) {
            fprintf(stderr, "Wrong arguments passed for this type of operation\n");
            exit(EXIT_FAILURE);
        }
    }
    else {
        fprintf(stderr, "Invalid operation type\n");
        exit(EXIT_FAILURE);
    }

    // build the request message, in TLV format
    tlv_request_t tlvRequestMsg;
    tlvRequestMsg.type = opCode;
    tlvRequestMsg.length = sizeof(msgValue);
    tlvRequestMsg.value = msgValue;
    

    // open server FIFO
    int fdFifoServer = open(SERVER_FIFO_PATH, O_WRONLY);
    if(fdFifoServer == -1) {
        perror("Open server FIFO");
        exit(EXIT_FAILURE);
    }

    // send request to server program
    if(write(fdFifoServer, &tlvRequestMsg, sizeof(tlv_request_t)) < 0) {
        perror("Write request to server");
        exit(EXIT_FAILURE);
    }


    // opens user FIFO
    int fdFifoUser = open(fifoName, O_RDONLY);
    if(fdFifoUser == -1) {
        perror("Open user FIFO");
        exit(EXIT_FAILURE);
    }

    // FALTA RECEBER RESPOSTA DO SERVER, ATRAVES DO USER FIFO
    // (OU ACABAR O PROGRAMA PASSADOS FIFO_TIMEOUT_SECS)



    // close and destroy file descriptors and user FIFO
    if(close(fdFifoUser) != 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    if(close(fdFifoServer) != 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    if(unlink(fifoName) != 0) {
        perror("unlink");
        exit(EXIT_FAILURE);
    }

    return 0;
}
