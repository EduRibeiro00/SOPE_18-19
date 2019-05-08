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

// global variables
pid_t pid;
int fdFifoUser;
int fdFifoServer;
int fdUserLogFile;
char fifoName[500];
tlv_reply_t replyMsg;

// ---------------------------------

int main(int argc, char* argv[]) {

    if(argc != 6) {
        fprintf(stderr, "Illegal use of arguments! Usage: %s <ID> <\"password\"> <op_delay> <op_code> <args> \n", argv[0]);
    }

    // calls the handler installer, for SIGALRM
    handlerInstaller();

    // opens (and creates, if necessary) user log file
    fdUserLogFile = open(USER_LOGFILE, O_WRONLY | O_CREAT | O_APPEND, 0664);
    if(fdUserLogFile == -1) {
        perror("Open user log file");
        exit(EXIT_FAILURE);
    }

    pid = getpid();

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

    // check if ID is in the valid range (if it is not, it will never correspond to an account)
    if(msgHeader.account_id < ADMIN_ACCOUNT_ID || msgHeader.account_id >= MAX_BANK_ACCOUNTS) {
        printf("Account ID is not in the valid range!\n");
        exit(EXIT_FAILURE);
    }

    // check if passoword is in the valid length range (if it is not, it will never be a password for an account)
    if(strlen(msgHeader.password) < MIN_PASSWORD_LEN || strlen(msgHeader.password) > MAX_PASSWORD_LEN) {
        printf("Illegal password length!\n");
        exit(EXIT_FAILURE);
    }

    msgValue.header = msgHeader;

    
    tlv_request_t tlvRequestMsg;
    tlvRequestMsg.type = atoi(argv[4]);

    tlvRequestMsg.length = sizeof(req_header_t);


    if(tlvRequestMsg.type == OP_CREATE_ACCOUNT) { // if create operation

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


        // verification of the argument values
        if(strlen(msgArgs.password) < MIN_PASSWORD_LEN || strlen(msgArgs.password) > MAX_PASSWORD_LEN) {
            printf("Illegal password length!\n");
            exit(EXIT_FAILURE);
        }

        if(msgArgs.balance < MIN_BALANCE || msgArgs.balance > MAX_BALANCE) {
            printf("Invalid account balance!\n");
            exit(EXIT_FAILURE);
        }

        if(msgArgs.account_id < 1 || msgArgs.account_id >= MAX_BANK_ACCOUNTS) {
            printf("New account cannot be created with that id!\n");
            exit(EXIT_FAILURE);
        }



        msgValue.create = msgArgs;
        tlvRequestMsg.length += sizeof(req_create_account_t);
    }
    else if(tlvRequestMsg.type == OP_TRANSFER) { // if transfer operation

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



        // verification of the argument values
        if(msgArgs.account_id < 1 || msgArgs.account_id >= MAX_BANK_ACCOUNTS) {
            printf("Invalid ID for the destination account!\n");
            exit(EXIT_FAILURE);
        }

        if(msgArgs.amount < MIN_BALANCE || msgArgs.amount > MAX_BALANCE) {
            printf("Invalid amount value!\n");
            exit(EXIT_FAILURE);
        }



        msgValue.transfer = msgArgs;
        tlvRequestMsg.length += sizeof(req_transfer_t);
    }
    else if(tlvRequestMsg.type == OP_BALANCE || tlvRequestMsg.type == OP_SHUTDOWN) {
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
    tlvRequestMsg.value = msgValue;

    // initializes reply message struct 
    initReply(tlvRequestMsg, &replyMsg);

    // open server FIFO
    fdFifoServer = open(SERVER_FIFO_PATH, O_WRONLY);
    if(fdFifoServer == -1) {
        // perror("Open server FIFO");
        // exit(EXIT_FAILURE);
        replyMsg.value.header.ret_code = RC_SRV_DOWN;
    }

    // writes request in user log file
    if(logRequest(fdUserLogFile, pid, &tlvRequestMsg) < 0) {
        perror("Write in user log file");
        exit(EXIT_FAILURE);
    }

    if(replyMsg.value.header.ret_code !=  RC_SRV_DOWN) {

        int totalLength = sizeof(op_type_t) + sizeof(uint32_t) + tlvRequestMsg.length;
        int n;

        // send request to server program
        if((n = write(fdFifoServer, &tlvRequestMsg, totalLength)) < 0) {
            perror("Write request to server");
            exit(EXIT_FAILURE);
        }

        // SIGALRM is sent after FIFO_TIMEOUT_SECS, if server did not reply
        alarm(FIFO_TIMEOUT_SECS);

        // opens user FIFO
        fdFifoUser = open(fifoName, O_RDONLY);
        if(fdFifoUser == -1) {
            //perror("Open user FIFO");
            // exit(EXIT_FAILURE);
        }

        readReply(&replyMsg, fdFifoUser);

        // cancels the alarm, because the reply was given before FIFO_TIMEOUT_SECS
        alarm(0);

        // close file descriptors
        if(close(fdFifoUser) != 0) {
            perror("close");
            exit(EXIT_FAILURE);
        }

        if(close(fdFifoServer) != 0) {
            perror("close");
            exit(EXIT_FAILURE);
        }
    }

    // writes reply in user log file
    if(logReply(fdUserLogFile, pid, &replyMsg) < 0) {
        perror("Write in user log file");
        exit(EXIT_FAILURE);
    }

    // destroy user FIFO
    if(unlink(fifoName) != 0) {
        perror("unlink");
        exit(EXIT_FAILURE);
    }

    return 0;
}

// ---------------------------------

void readReply(tlv_reply_t* reply, int fdUserFifo) {
    
    int n;

    // reads request type
    if((n = read(fdUserFifo, &(reply->type), sizeof(op_type_t))) < 0) {
        perror("Read request type");
        exit(EXIT_FAILURE);
    }


    // reads request length
    if((n = read(fdUserFifo, &(reply->length), sizeof(uint32_t))) < 0) {
        perror("Read request length");
        exit(EXIT_FAILURE);
    }


    // reads request value
    if((n = read(fdUserFifo, &(reply->value), reply->length)) < 0) {
        perror("Read request value");
        exit(EXIT_FAILURE);
    }

}

// ---------------------------------

void alarmHandler(int signo) {
    if(signo == SIGALRM) {
        replyMsg.value.header.ret_code = RC_SRV_TIMEOUT;

        // close file descriptors
        if(close(fdFifoUser) != 0) {
            perror("close");
            exit(EXIT_FAILURE);
        }

        if(close(fdFifoServer) != 0) {
            perror("close");
            exit(EXIT_FAILURE);
        }

        // writes reply in user log file
        if(logReply(fdUserLogFile, pid, &replyMsg) < 0) {
            perror("Write in user log file");
            exit(EXIT_FAILURE);
        }

        // destroy user FIFO
        if(unlink(fifoName) != 0) {
            perror("unlink");
            exit(EXIT_FAILURE);
        }
    }
}

// ---------------------------------

void handlerInstaller() {

    struct sigaction action;
    action.sa_handler = alarmHandler;
    
    if(sigemptyset(&action.sa_mask) == -1) {
        perror("sigemptyset");
        exit(EXIT_FAILURE);
    }

    action.sa_flags = SA_RESTART;

    if(sigaction(SIGALRM, &action, NULL) != 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}
