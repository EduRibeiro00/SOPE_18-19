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

// global variables
bank_account_t accounts[MAX_BANK_ACCOUNTS];
tlv_request_t requests[30];
//--------------------------

int main(int argc, char* argv[]) {

// -----------------------
// parte de teste

    /*accounts[0].account_id = 0;
    strcpy(accounts[0].hash, "1960ac94c29be96599d4ee2d111221344ba9583d073e42d51a965a5020502489");
    strcpy(accounts[0].salt, "12345fba213acb2");


    if(checkPassword(0, "ola123"))
        printf("passou\n");
    else printf("nao passou\n");*/

// ------------------------

    if(argc != 3) {
        fprintf(stderr, "Illegal use of arguments! Usage: %s <numThreads> <adminPass>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // opens (and creates, if necessary) server log file
    // int serverLogFile = open(SERVER_LOGFILE, O_WRONLY | O_CREAT, 0664);

    createAdminAccount(argv[2]);

    // ---------------
    // FAZER OS THREADS DEPOIS
    // ---------------

    if(mkfifo(SERVER_FIFO_PATH, 0660) < 0) {
        if(errno == EEXIST) 
            fprintf(stderr, "FIFO '%s' already exists\n", SERVER_FIFO_PATH);
        else {
            fprintf(stderr, "Can't create server FIFO!");
            exit(EXIT_FAILURE);
        }
    }

    int fdFifoServer;
    int fdFifoServerDummy; // to avoid busy waiting
    // int fdFifoUser;

    if((fdFifoServer = open(SERVER_FIFO_PATH, O_RDONLY)) == -1) {
        perror("Open server FIFO to read");
        exit(EXIT_FAILURE);
    }

    if((fdFifoServerDummy = open(SERVER_FIFO_PATH, O_WRONLY)) == -1) {
        perror("Open server FIFO to write");
        exit(EXIT_FAILURE);
    }

    tlv_request_t request;

    int i = 1; // TEMPORARIO; DEPOIS CICLO SO DEVE TERMINAR SE O PEDIDO FOR DE ENCERRAMENTO
               // (MUDAR PERMISSOES DO FIFO SERVER PARA SO LEITURA)

    // reads requests
    do {
        readRequest(&request, fdFifoServer);
        i = printRequest(request);
    } while(i);


    if(close(fdFifoServer) != 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    if(close(fdFifoServerDummy) != 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    if(unlink(SERVER_FIFO_PATH) != 0) {
        perror("unlink");
        exit(EXIT_FAILURE);
    }

    return 0;
}

// ---------------------------------

// int createThreads(pthread_t* tids, int numThreads) {

//     for(int i = 0; i < numThreads; i++) {
//         // pthread_create(&tids[i], NULL, func, arg);
//     }
// }

// ---------------------------------

bool checkPassword(uint32_t id, char* password) {
    
    char hash[HASH_LEN + 1];
    char salt[SALT_LEN + 1];
    char hashResult[HASH_LEN + 1];

    // account doesn't exist
    if(!(getHashFromAccount(id, hash) && getSaltFromAccount(id, salt)))
        return false;

    // checks if hash if equal to the generated hash
    generateHash(password, salt, hashResult);
    return (!strcmp(hash, hashResult));
}

// ---------------------------------

bank_account_t* getBankAccount(uint32_t id) {
    
    for(int i = 0; i < MAX_BANK_ACCOUNTS; i++) {
        if(accounts[i].account_id == id)
            return &accounts[i];
    }

    return NULL;
}

// ---------------------------------

bool getSaltFromAccount(uint32_t id, char* salt) {

    bank_account_t* account = getBankAccount(id);
    if(account == NULL)
        return false;

    strcpy(salt, account->salt);
    return true;
}

// ---------------------------------

bool getHashFromAccount(uint32_t id, char* hash) {

    bank_account_t* account = getBankAccount(id);
    if(account == NULL)
        return false;

    strcpy(hash, account->hash);
    return true;
}

// ---------------------------------

void createAdminAccount(char* password) {
    accounts[0].account_id = ADMIN_ACCOUNT_ID;
    accounts[0].balance = 0;

    char salt[SALT_LEN + 1];
    char hash[HASH_LEN + 1];

    generateRandomSalt(salt);
    generateHash(password, salt, hash);

    strcpy(accounts[0].salt, salt);
    strcpy(accounts[0].hash, hash);
}

// ---------------------------------

void readRequest(tlv_request_t* request, int fdFifoServer) {

    int n;

    // reads request type
    if((n = read(fdFifoServer, &(request->type), sizeof(op_type_t))) < 0) {
        perror("Read request type");
        exit(EXIT_FAILURE);
    }


    // reads request length
    if((n = read(fdFifoServer, &(request->length), sizeof(uint32_t))) < 0) {
        perror("Read request length");
        exit(EXIT_FAILURE);
    }


    // reads request value
    if((n = read(fdFifoServer, &(request->value), request->length)) < 0) {
        perror("Read request value");
        exit(EXIT_FAILURE);
    }
}


// ---------------------------------

bool isAdmin(uint32_t id);
