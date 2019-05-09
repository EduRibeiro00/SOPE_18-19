#pragma once

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

// macro that defines the maximum number of requests that can be store in the requests array
#define MAX_REQUESTS    30

// creates the threads (bank offices) in the beginning
void createBankOffices(bank_office_t* bankOffices, int numThreads);

// closes the bank offices
void closeBankOffices(bank_office_t* bankOffices, int numThreads);


// checks if a password that a user send the server is valid
bool checkPassword(uint32_t id, char* password);

// gets bank account from main array; returns NULL if account doesn't exist
bank_account_t* getBankAccount(uint32_t id);

// returns salt from account; returns false if account doesn't exist
bool getSaltFromAccount(uint32_t id, char* salt);

// returns hash from account; returns false if account doesn't exist
bool getHashFromAccount(uint32_t id, char* hash); 

// returns balance from account; returns false if account doesn't exist
bool getBalanceFromAccount(uint32_t id, uint32_t* balance);

// creates the admin account, in the beginning of the program
void createAdminAccount(char* password);

// reads a user request from the server FIFO 
void readRequest(tlv_request_t* request);

// writes a reply message to the user FIFO
void writeReply(tlv_reply_t* reply, int fdFifoUser);

// function that creates a new bank account
void createAccount(uint32_t id, char* password, int balance);

// return true if id is the admin id
bool isAdmin(uint32_t id);

// transfers a specific amount from one account to other; assumes all needed validation and verification was done
// before the function call
void transferAmount(bank_account_t* sourceAccount, bank_account_t* destAccount, uint32_t amount);



// handles and fulfills a user request
void handleRequest(tlv_request_t request, tlv_reply_t* reply);

// handles a "create account" request
void handleCreateAccount(req_value_t value, tlv_reply_t* reply);

// handles a "check balance" request
void handleCheckBalance(req_value_t value, tlv_reply_t* reply);

//handles a "transfer" request
void handleTransfer(req_value_t value, tlv_reply_t* reply); 

// handles a "shutdown" request
void handleShutdown(req_value_t value, tlv_reply_t* reply);


// struct to keep the information about the bank offices (threads) created
typedef struct bank_office {
    pthread_t tid;
    int id;
} bank_office_t;


// thread function; gets the requests from the array, processes them, and sends the result to the user
void* bankOfficeFunction(void* arg);