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
#include <pthread.h>

#include "constants.h"
#include "sope.h"
#include "types.h"
#include "aux.h"
#include "server.h"

// global variables
bank_account_t accounts[MAX_BANK_ACCOUNTS];
int numAccounts = 0;

tlv_request_t requests[MAX_REQUESTS];
int bufferIn = 0, bufferOut = 0, items = 0, slots = MAX_REQUESTS;

int shutdownFlag = 0; // turns 1 when received a shutdown request

int fdFifoServer;
int fdServerLogFile;

// sync mechanisms - producer/consumer
pthread_mutex_t buffer_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t slots_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t slots_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t items_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t items_cond = PTHREAD_COND_INITIALIZER;

// sync mechanisms - bank account array - one mutex per account (ex: mutex for account with id 2 is in index 2 of this array)
pthread_mutex_t accountMutexes[MAX_BANK_ACCOUNTS];
//--------------------------

int main(int argc, char* argv[]) {

    if(argc != 3) {
        fprintf(stderr, "Illegal use of arguments! Usage: %s <numThreads> <adminPass>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));

    // opens (and creates, if necessary) server log file
    fdServerLogFile = open(SERVER_LOGFILE, O_WRONLY | O_CREAT | O_APPEND, 0664);
    if(fdServerLogFile == -1) {
        perror("Open server log file");
        exit(EXIT_FAILURE);
    }

    // initialize account mutexes
    for(int i = 0; i < MAX_BANK_ACCOUNTS; i++)
        pthread_mutex_init(&accountMutexes[i], NULL);

    createAdminAccount(argv[2]);

    // thread creation
    int numBankOffices = atoi(argv[1]);
    bank_office_t bankOffices[numBankOffices];
    createBankOffices(bankOffices, numBankOffices);


    if(mkfifo(SERVER_FIFO_PATH, 0666) < 0) {
        if(errno == EEXIST) 
            fprintf(stderr, "FIFO '%s' already exists\n", SERVER_FIFO_PATH);
        else {
            fprintf(stderr, "Can't create server FIFO!");
            exit(EXIT_FAILURE);
        }
    }


    int fdFifoServerDummy; // to avoid busy waiting

    if((fdFifoServer = open(SERVER_FIFO_PATH, O_RDONLY)) == -1) {
        perror("Open server FIFO to read");
        exit(EXIT_FAILURE);
    }

    if((fdFifoServerDummy = open(SERVER_FIFO_PATH, O_WRONLY)) == -1) {
        perror("Open server FIFO to write");
        exit(EXIT_FAILURE);
    }


    // reads the requests and sends them to the request queue
    do {
        syncSlotsMainThread();

        tlv_request_t currentRequest;

        // reads the request from a user, and sends it to the request queue
        readRequest(&currentRequest);
        int requestPid = currentRequest.value.header.pid;

        // tries to gain access to the buffer
        pthread_mutex_lock(&buffer_lock);

        if(logSyncMech(fdServerLogFile, MAIN_THREAD_ID, SYNC_OP_MUTEX_LOCK, SYNC_ROLE_PRODUCER, 0) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }

        requests[bufferIn] = currentRequest;
        bufferIn = (bufferIn + 1) % MAX_REQUESTS;

        if(logRequest(fdServerLogFile, MAIN_THREAD_ID, &currentRequest) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_unlock(&buffer_lock);

        if(logSyncMech(fdServerLogFile, MAIN_THREAD_ID, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_PRODUCER, requestPid) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }

        syncItemsMainThread(requestPid);

    } while(!shutdownFlag); // (temporario; n e bem assim q deve ser feito no fim) (?)


    closeBankOffices(bankOffices, numBankOffices);

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

    destroySync();

    return 0;
}

// ---------------------------------

void* bankOfficeFunction(void* arg) {

    int bankOfficeId = *(int*) arg;

    while(!shutdownFlag) {
        
        syncItemsBankOffice(bankOfficeId);

        pthread_mutex_lock(&buffer_lock);

        if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_LOCK, SYNC_ROLE_CONSUMER, 0) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }

        tlv_request_t currentRequest = requests[bufferOut];
        bufferOut = (bufferOut + 1) % MAX_REQUESTS;

        if(logRequest(fdServerLogFile, bankOfficeId, &currentRequest) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_unlock(&buffer_lock);

        if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_CONSUMER, currentRequest.value.header.pid) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }

        syncSlotsBankOffice(bankOfficeId, currentRequest.value.header.pid);

        tlv_reply_t reply;

        handleRequest(currentRequest, &reply, bankOfficeId);

        char fifoNameUser[500];
        sprintf(fifoNameUser, "%s%d", USER_FIFO_PATH_PREFIX, currentRequest.value.header.pid);

        int fdFifoUser;

        // opens user fifo to send reply message
        if((fdFifoUser = open(fifoNameUser, O_WRONLY)) == -1) {
            // perror("Open user FIFO to write");
            reply.value.header.ret_code = RC_USR_DOWN;
        }

        // sends reply (if user FIFO was open)
        if(reply.value.header.ret_code != RC_USR_DOWN) {
        
            writeReply(&reply, fdFifoUser);

            // closes user FIFO
            if(close(fdFifoUser) != 0) {
                perror("close");
                exit(EXIT_FAILURE);
            }
        }

        if(logReply(fdServerLogFile, bankOfficeId, &reply) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }

    }

    pthread_exit(NULL);
}

// ---------------------------------

void createBankOffices(bank_office_t* bankOffices, int numThreads) {

    for(int i = 0; i < numThreads; i++) {
        bankOffices[i].id = i + 1;
        bankOffices[i].processing = false;
        pthread_create(&bankOffices[i].tid, NULL, bankOfficeFunction, (void*) &bankOffices[i].id);

        if(logBankOfficeOpen(fdServerLogFile, bankOffices[i].id, bankOffices[i].tid) < 0) {
            perror("write to server log file");
            exit(EXIT_FAILURE);
        }
    }
}

// ---------------------------------

void closeBankOffices(bank_office_t* bankOffices, int numThreads) {

    for(int i = 0; i < numThreads; i++) {
        
        pthread_join(bankOffices[i].tid, NULL);

        if(logBankOfficeClose(fdServerLogFile, bankOffices[i].id, bankOffices[i].tid) < 0) {
            perror("write to server log file");
            exit(EXIT_FAILURE);
        }
    }
}

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

    if (hasSpaces(password)) {
        printf("Password must not contain spaces!\n");
        exit(EXIT_FAILURE);
    }

    createAccount(ADMIN_ACCOUNT_ID, password, 0);

    bank_account_t* adminAccount = getBankAccount(ADMIN_ACCOUNT_ID);
    if(adminAccount != NULL) {
        if(logAccountCreation(fdServerLogFile, MAIN_THREAD_ID, adminAccount) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }
    }
    else {
        fprintf(stderr, "Admin account couldn't be created!\n");
        exit(EXIT_FAILURE);
    }
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

void writeReply(tlv_reply_t* reply, int fdFifoUser) {

    int totalLength = sizeof(op_type_t) + sizeof(uint32_t) + reply->length;

    int n;

    // send request to server program
    if((n = write(fdFifoUser, reply, totalLength)) < 0) {
        perror("Write reply to user");
        exit(EXIT_FAILURE);
    }
}

// ---------------------------------

void handleRequest(tlv_request_t request, tlv_reply_t* reply, int bankOfficeId) {

    initReply(request, reply);

    switch(request.type) {

        case OP_CREATE_ACCOUNT:
            handleCreateAccount(request.value, reply, bankOfficeId);
            break;

        case OP_BALANCE:
            handleCheckBalance(request.value, reply, bankOfficeId);
            break;

        case OP_TRANSFER:
            handleTransfer(request.value, reply, bankOfficeId);
            break;     

        case OP_SHUTDOWN:
            handleShutdown(request.value, reply, bankOfficeId);
            break;

        default:
            break;        
    }
}

// ---------------------------------

void handleCreateAccount(req_value_t value, tlv_reply_t* reply, int bankOfficeId) {

    // exclusive access to admin account is not needed: it is always created in the beginning and
    // never changes. Even if an account is created while doing this operation, it doesn't matter
    // because this needs the admin account to work.

    // authentication
    if(!checkPassword(value.header.account_id, value.header.password)) {
        printf("The id and password combination is invalid.\n");
        reply->value.header.ret_code = RC_LOGIN_FAIL;     
        return;
    }


    if(!isAdmin(value.header.account_id)) {
        printf("User is not admin; doesn't have permission!\n");
        reply->value.header.ret_code = RC_OP_NALLOW; 
        return;
    }

    // if there is no more space for bank accounts, and each one has a different id from 0-MAX_BANK_ACCOUNTS,
    // then it means that the id given (that is valid) is already being used.
    if(numAccounts >= MAX_BANK_ACCOUNTS) {
        printf("The max number of bank accounts has been reached!\n");
        reply->value.header.ret_code = RC_ID_IN_USE;
        return;
    }

    // get access to the account
    pthread_mutex_lock(&accountMutexes[value.create.account_id]);

    if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_LOCK, SYNC_ROLE_ACCOUNT, value.create.account_id) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }

    // delay is in milisseconds; usleep() needs microsseconds
    usleep(value.header.op_delay_ms * 1000);

    if (logSyncDelay(fdServerLogFile, bankOfficeId, value.create.account_id, value.header.op_delay_ms) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }

    // verifies if the bank account already exists
    if(getBankAccount(value.create.account_id) != NULL) {
        printf("Account already exists!\n");
        reply->value.header.ret_code = RC_ID_IN_USE;
        
        pthread_mutex_unlock(&accountMutexes[value.create.account_id]);

        if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, value.create.account_id) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }
        
        return;
    }

    createAccount(value.create.account_id, value.create.password, value.create.balance);
    reply->value.header.ret_code = RC_OK;

    bank_account_t* newAccount = getBankAccount(value.create.account_id);
    if(newAccount != NULL) {
        if (logAccountCreation(fdServerLogFile, bankOfficeId, newAccount) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }
    }
    else // for some reason, the account wasn't created sucessfully
        reply->value.header.ret_code = RC_OTHER;

    pthread_mutex_unlock(&accountMutexes[value.create.account_id]);

    if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, value.create.account_id) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }
}

// ---------------------------------

void handleCheckBalance(req_value_t value, tlv_reply_t* reply, int bankOfficeId) {

    pthread_mutex_lock(&accountMutexes[value.header.account_id]);

    if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_LOCK, SYNC_ROLE_ACCOUNT, value.header.account_id) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }

    // delay is in milisseconds; usleep() needs microsseconds
    usleep(value.header.op_delay_ms * 1000);

    if(logSyncDelay(fdServerLogFile, bankOfficeId, value.header.account_id, value.header.op_delay_ms) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }

    // authentication
    if(!checkPassword(value.header.account_id, value.header.password)) {
        printf("The id and password combination is invalid.\n");
        reply->value.header.ret_code = RC_LOGIN_FAIL;

        pthread_mutex_unlock(&accountMutexes[value.header.account_id]);

        if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, value.header.account_id) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }

        return;
    }

    if(isAdmin(value.header.account_id)) {
        printf("User is admin; can't perform this operation!\n");
        reply->value.header.ret_code = RC_OP_NALLOW;

        pthread_mutex_unlock(&accountMutexes[value.header.account_id]);

        if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, value.header.account_id) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }

        return;
    }
        
    uint32_t balance;

    // will always return true because if authentication was sucessful, means that account exists
    getBalanceFromAccount(value.header.account_id, &balance);


    pthread_mutex_unlock(&accountMutexes[value.header.account_id]);

    if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, value.header.account_id) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }

    reply->value.header.ret_code = RC_OK;
    reply->value.balance.balance = balance;
    reply->length += sizeof(rep_balance_t);    
}

// ---------------------------------

void handleTransfer(req_value_t value, tlv_reply_t* reply, int bankOfficeId) {
    
    // This cycle is done in order to prevent a deadlock. Because we are trying to gain access to two accounts,
    // this can cause a "circular wait" with other threads. To prevent that, we always gain access to the account
    // with the lowest id first, so that these cycles will never occur.

    uint32_t first = MIN(value.header.account_id, value.transfer.account_id);
    uint32_t second = MAX(value.header.account_id, value.transfer.account_id);

    pthread_mutex_lock(&accountMutexes[first]);

    if (logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_LOCK, SYNC_ROLE_ACCOUNT, first) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }

    // -- delay is in milisseconds; usleep() needs microsseconds
    usleep(value.header.op_delay_ms * 1000);

    if (logSyncDelay(fdServerLogFile, bankOfficeId, first, value.header.op_delay_ms) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }

    if (second != first) {
        pthread_mutex_lock(&accountMutexes[second]);

        if (logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_LOCK, SYNC_ROLE_ACCOUNT, second) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }

        // -- delay is in milisseconds; usleep() needs microsseconds
        usleep(value.header.op_delay_ms * 1000);

        if (logSyncDelay(fdServerLogFile, bankOfficeId, second, value.header.op_delay_ms) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }
    }


    // authentication
    if(!checkPassword(value.header.account_id, value.header.password)) {
        printf("The id and password combination is invalid.\n");
        reply->value.header.ret_code = RC_LOGIN_FAIL;

        if (second != first) {
            pthread_mutex_unlock(&accountMutexes[second]);

            if (logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, second) < 0) {
                perror("Write in server log file");
                exit(EXIT_FAILURE);
            }
        }


        pthread_mutex_unlock(&accountMutexes[first]);

        if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, first) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }


        return;
    }

    reply->value.header.ret_code = RC_OK;

    // checks if the source account is admin
    if(isAdmin(value.header.account_id)) {
        printf("User is admin; can't perform this operation!\n");
        reply->value.header.ret_code = RC_OP_NALLOW;
    }

    if(reply->value.header.ret_code != RC_OP_NALLOW) {

        bank_account_t* sourceAccount = getBankAccount(value.header.account_id);
        bank_account_t* destAccount = getBankAccount(value.transfer.account_id);


        if(destAccount == NULL) {
            printf("Destination account doesn't exist!\n");
            reply->value.header.ret_code = RC_ID_NOT_FOUND;
        }

        if((reply->value.header.ret_code == RC_OK) && (value.header.account_id == value.transfer.account_id)) {
            printf("The accounts are the same! Transfer not possible\n");
            reply->value.header.ret_code = RC_SAME_ID;
        }

        if((reply->value.header.ret_code == RC_OK) && (sourceAccount->balance < value.transfer.amount)) {
            printf("Not enough money to complete the transfer!\n");
            reply->value.header.ret_code = RC_NO_FUNDS;
        }

        if((reply->value.header.ret_code == RC_OK) && (destAccount != NULL && destAccount->balance + value.transfer.amount > MAX_BALANCE)) {
            printf("The destination account's balance would be too high!\n");
            reply->value.header.ret_code = RC_TOO_HIGH;
        }


        if(reply->value.header.ret_code == RC_OK)
            transferAmount(sourceAccount, destAccount, value.transfer.amount);   

    }

    if(reply->value.header.ret_code == RC_OK) {
        uint32_t balance;

        // will always return true because if authentication was sucessful, means that account exists
        getBalanceFromAccount(value.header.account_id, &balance);

        reply->value.transfer.balance = balance;
    }
    else {
        reply->value.transfer.balance = value.transfer.amount;
    }


    if (second != first) {
        pthread_mutex_unlock(&accountMutexes[second]);

        if (logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, second) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }
    }


    pthread_mutex_unlock(&accountMutexes[first]);

    if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, first) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }


    reply->length += sizeof(rep_transfer_t);
}

// ---------------------------------

void handleShutdown(req_value_t value, tlv_reply_t* reply, int bankOfficeId) {

    // authentication
    if(!checkPassword(value.header.account_id, value.header.password)) {
        printf("The id and password combination is invalid.\n");
        reply->value.header.ret_code = RC_LOGIN_FAIL;
        return;
    }

    if(!isAdmin(value.header.account_id)) {
        printf("User is not admin; doesn't have permission!\n");
        reply->value.header.ret_code = RC_OP_NALLOW;
        return;
    }

    // delay is in milisseconds; usleep() needs microsseconds
    usleep(value.header.op_delay_ms * 1000); // ANTES DE IMPEDIR QUE SE RECEBA MAIS PEDIDOS

    if (logDelay(fdServerLogFile, bankOfficeId, value.header.op_delay_ms) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }

    // changes FIFO permissions to read only, to avoid receiving new requests
    if(fchmod(fdFifoServer, S_IRUSR | S_IRGRP | S_IROTH ) < 0) {
        perror("fchmod");
        exit(EXIT_FAILURE);
    }
    
    reply->value.header.ret_code = RC_OK;

    // PROGRAMA SO DEVE TERMINAR QUANDO TODOS OS PEDIDOS RESTANTES FOREM PROCESSADOS
    
    // RETORNAR TAMBEM O NUMERO DE THREADS ATIVAS (A PROCESSAR UM PEDIDO) NO MOMENTO DO ENVIO.
    reply->value.shutdown.active_offices = 1; // por agora fica 1, como valor de teste
    reply->length += sizeof(rep_shutdown_t);

    shutdownFlag = 1;
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

// ---------------------------------

void syncSlotsMainThread() {

    // tries to gain access to a slot
    pthread_mutex_lock(&slots_lock);
    
    if(logSyncMech(fdServerLogFile, MAIN_THREAD_ID, SYNC_OP_MUTEX_LOCK, SYNC_ROLE_PRODUCER, 0) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }

    // has to wait if there are no slots available
    while(slots <= 0) {
        pthread_cond_wait(&slots_cond, &slots_lock);
    
        if(logSyncMech(fdServerLogFile, MAIN_THREAD_ID, SYNC_OP_COND_WAIT, SYNC_ROLE_PRODUCER, 0) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }
    }
    
    slots--;
    pthread_mutex_unlock(&slots_lock);

    if(logSyncMech(fdServerLogFile, MAIN_THREAD_ID, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_PRODUCER, 0) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }
}

// ---------------------------------

void syncItemsMainThread(int requestPid) {

    // release right to an item
    pthread_mutex_lock(&items_lock);

    if(logSyncMech(fdServerLogFile, MAIN_THREAD_ID, SYNC_OP_MUTEX_LOCK, SYNC_ROLE_PRODUCER, requestPid) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }

    items++;
    pthread_cond_signal(&items_cond);

    if(logSyncMech(fdServerLogFile, MAIN_THREAD_ID, SYNC_OP_COND_SIGNAL, SYNC_ROLE_PRODUCER, requestPid) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_unlock(&items_lock);

    if(logSyncMech(fdServerLogFile, MAIN_THREAD_ID, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_PRODUCER, requestPid) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }
}

// ---------------------------------

void syncSlotsBankOffice(int bankOfficeId, int requestPid) {

    // release a slot space
    pthread_mutex_lock(&slots_lock);

    if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_LOCK, SYNC_ROLE_CONSUMER, requestPid) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }

    slots++;
    pthread_cond_signal(&slots_cond);

    if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_COND_SIGNAL, SYNC_ROLE_CONSUMER, requestPid) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_unlock(&slots_lock);

    if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_CONSUMER, requestPid) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }
}

// ---------------------------------

void syncItemsBankOffice(int bankOfficeId) {

    pthread_mutex_lock(&items_lock);

    if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_LOCK, SYNC_ROLE_CONSUMER, 0) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }

    // waits until there are requests to collect and process
    while(items <= 0) {
        pthread_cond_wait(&items_cond, &items_lock);
        
        if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_COND_WAIT, SYNC_ROLE_CONSUMER, 0) < 0) {
            perror("Write in server log file");
            exit(EXIT_FAILURE);
        }
    }

    items--;
    pthread_mutex_unlock(&items_lock);

    if(logSyncMech(fdServerLogFile, bankOfficeId, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_CONSUMER, 0) < 0) {
        perror("Write in server log file");
        exit(EXIT_FAILURE);
    }
}

// ---------------------------------

void destroySync() {

    for (size_t i = 0; i < MAX_BANK_ACCOUNTS; i++) {
        pthread_mutex_destroy(&accountMutexes[i]);
    }

    pthread_mutex_destroy(&buffer_lock);
    pthread_mutex_destroy(&slots_lock);
    pthread_cond_destroy(&slots_cond);
    pthread_mutex_destroy(&items_lock);
    pthread_cond_destroy(&items_cond);
    
}
