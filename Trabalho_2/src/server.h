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

// creates the threads in the beginning
// int createThreads(pthread_t* tids, int numThreads);

// checks if a password that a user send the server is valid
bool checkPassword(uint32_t id, char* password);

// gets bank account from main array; returns NULL if account doesn't exist
bank_account_t* getBankAccount(uint32_t id);

// returns salt from account; returns false if account doesn't exist
bool getSaltFromAccount(uint32_t id, char* salt);

// returns hash from account; returns false if account doesn't exist
bool getHashFromAccount(uint32_t id, char* hash); 

// creates the admin account, in the beginning of the program
void createAdminAccount(char* password);

bool isAdmin(uint32_t id);
