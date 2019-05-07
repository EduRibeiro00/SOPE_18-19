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
int numAccounts = 0;

tlv_request_t requests[30];
int bufferIn = 0, bufferOut = 0;

int fdFifoServer;
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
        readRequest(&request);
        handleRequest(request);

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

    // checks if hash is equal to the generated hash
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

bool getBalanceFromAccount(uint32_t id, uint32_t* balance) {

    bank_account_t* account = getBankAccount(id);
    if(account == NULL)
        return false;

    *balance = account->balance;
    return true;
}

// ---------------------------------

void createAdminAccount(char* password) {

    if(strlen(password) < MIN_PASSWORD_LEN || strlen(password) > MAX_PASSWORD_LEN) {
        printf("Illegal password length!\n");
        exit(EXIT_FAILURE);
    }

    createAccount(ADMIN_ACCOUNT_ID, password, 0);
}

// ---------------------------------

void readRequest(tlv_request_t* request) {

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

void handleRequest(tlv_request_t request) {

    // delay is in milisseconds; usleep() needs microsseconds
    usleep(request.value.header.op_delay_ms * 1000);

    // autentication
    if(!checkPassword(request.value.header.account_id, request.value.header.password)) {
        printf("The id and password combination is invalid.\n");
        return;
    }

    switch(request.type) {

        case OP_CREATE_ACCOUNT:
            handleCreateAccount(request.value);
            break;

        case OP_BALANCE:
            handleCheckBalance(request.value);
            break;

        case OP_TRANSFER:
            handleTransfer(request.value);
            break;     

        case OP_SHUTDOWN:
            handleShutdown(request.value);

        default:
            break;        
    }
}

// ---------------------------------

void handleCreateAccount(req_value_t value) {

    if(numAccounts >= MAX_BANK_ACCOUNTS) {
        printf("The max number of bank accounts has been reached!\n");
        return;
    }

    // op_nallow
    if(!isAdmin(value.header.account_id)) {
        printf("User is not admin; doesn't have permission!\n");
        return;
    }

    // id_in_use
    if(getBankAccount(value.create.account_id) != NULL) {
        printf("Account already exists!\n");
        return;
    }

    createAccount(value.create.account_id, value.create.password, value.create.balance);
}

// ---------------------------------

void handleCheckBalance(req_value_t value) {

    // op_nallow
    if(isAdmin(value.header.account_id)) {
        printf("User is admin; can't perform this operation!\n");
        return;
    }

    uint32_t balance;

    // will always return true because if autentication was sucessfull, means that account exists
    getBalanceFromAccount(value.header.account_id, &balance);

    // RETORNAR SALDO
}

// ---------------------------------

void handleTransfer(req_value_t value) {
    
    // op_nallow
    if(isAdmin(value.header.account_id)) {
        printf("User is admin; can't perform this operation!\n");
        return;
    }

    bank_account_t* sourceAccount = getBankAccount(value.header.account_id);
    bank_account_t* destAccount = getBankAccount(value.transfer.account_id);
    
    // id_not_found
    if(destAccount == NULL) {
        printf("Destination account doesn't exist!\n");
        return;
    }

    // same_id
    if(value.header.account_id == value.transfer.account_id) {
        printf("The accounts are the same! Transfer not possible\n");
        return;
    }

    // no_funds
    if(sourceAccount->balance < value.transfer.amount) {
        printf("Not enough money to complete the transfer!\n");
        return;
    }

    // too_high
    if(destAccount->balance + value.transfer.amount > MAX_BALANCE) {
        printf("The destination account's balance would be too high!\n");
        return;
    }

    transferAmount(sourceAccount, destAccount, value.transfer.amount);


    // RETORNAR SALDO DA CONTA SOURCE
}

// ---------------------------------

void handleShutdown(req_value_t value) {

    // op_nallow
    if(!isAdmin(value.header.account_id)) {
        printf("User is not admin; doesn't have permission!\n");
        return;
    }

    // changes FIFO permissions to read only, to avoid receiving new requests
    if(fchmod(fdFifoServer, S_IRUSR | S_IRGRP | S_IROTH ) < 0) {
        perror("fchmod");
        exit(EXIT_FAILURE);
    }

    // PROGRAMA SO DEVE TERMINAR QUANDO TODOS OS PEDIDOS RESTANTES FOREM PROCESSADOS
    
    // RETORNAR TAMBEM O NUMERO DE THREADS ATIVAS (A PROCESSAR UM PEDIDO) NO MOMENTO DO ENVIO. 
}

// ---------------------------------

void createAccount(uint32_t id, char* password, int balance) {
    accounts[numAccounts].account_id = id;
    accounts[numAccounts].balance = balance;

    char salt[SALT_LEN + 1];
    char hash[HASH_LEN + 1];

    generateRandomSalt(salt);
    generateHash(password, salt, hash);

    strcpy(accounts[numAccounts].salt, salt);
    strcpy(accounts[numAccounts].hash, hash);

    numAccounts++;
}

// ---------------------------------

void transferAmount(bank_account_t* sourceAccount, bank_account_t* destAccount, uint32_t amount) {
    sourceAccount->balance -= amount;
    destAccount->balance += amount;
}

// ---------------------------------

bool isAdmin(uint32_t id) {
    return id == ADMIN_ACCOUNT_ID;
}
