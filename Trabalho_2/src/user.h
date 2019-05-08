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

// reads from the user FIFO the reply
void readReply(tlv_reply_t* reply, int fdUserFifo);

// SIGALRM handler; for the user 
void alarmHandler(int signo);

// function that installs the SIGALRM signal handler
void handlerInstaller();
