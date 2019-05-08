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

// macros for pipes
#define READ  0
#define WRITE 1


// string has to be SALT_LEN + 1 length
void generateRandomSalt(char* salt);

// generates hash for a given password and salt; uses coprocess (sha256sum)
void generateHash(char* password, char* salt, char* hashResult);

// fills the reply struct with the initial values
void initReply(tlv_request_t request, tlv_reply_t* reply);



// test function; just to see if server is getting the user requests
// returns 0 (false) if shutdown request; 1 (true otherwise)
int printRequest(tlv_request_t request);

// for test purposes; prints the reply gotten from the server 
void printReply(tlv_reply_t reply);
