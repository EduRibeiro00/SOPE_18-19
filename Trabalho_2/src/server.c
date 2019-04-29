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
//--------------------------

int main() {

    if(checkPassword("ola123"))
        printf("passou\n");
    else printf("nao passou\n");

    return 0;
}

// ---------------------------------

// int createThreads(pthread_t* tids, int numThreads) {

//     for(int i = 0; i < numThreads; i++) {
//         // pthread_create(&tids[i], NULL, func, arg);
//     }
// }

// ---------------------------------

bool checkPassword(char* password) {
    
    char hash[HASH_LEN + 1];

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