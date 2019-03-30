#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h> 

#define REC         0
#define MD5         1
#define SHA1        2
#define SHA256      3
#define OUT         4
#define LOG         5
#define NUM_OPTIONS 6

#define MAX_SIZE    510

//global variables:

DIR* dir; //pointer to the current directory that is being analyzed (if any)
bool boolArray[NUM_OPTIONS]; //array containing different values concerning the options given by the command
int fdOut; //descriptor of the output file (if there is any)
int fdLog; //descriptor of the log file (if there is any)
struct timeval startTime; //initial starting time of the main process

//-------------------------

void commandToString(char* result, int max_size, char* command, char* file){

        FILE* fileOut;

        char pipeCommand[MAX_SIZE];
        strcpy(pipeCommand, command);
        strcat(pipeCommand, " ");
        strcat(pipeCommand, file);

        if((fileOut = popen(pipeCommand, "r")) == NULL){ //uses a pipe in order to get the output of the command
            fprintf(stderr, "Popen error!\n");
            exit(3);
        }

        if(fgets(result, max_size, fileOut) == NULL){ //extracts the result of the command from the file pointer
            fprintf(stderr, "Error with fgets!\n");
            exit(3);
        }

        result[strlen(result) - 1] = '\0'; //gets rid of the newline

        if(pclose(fileOut) != 0){
            fprintf(stderr, "Pclose error!\n");
            exit(3);
        }
}

//-------------------------

void getFileAcess(char* result, int size, mode_t mode){

    memset(result, 0, size);
    bool flag = false;

    if(mode & S_IRUSR){ //user read access
        strcat(result, "r");
        flag = true;
    }

    if(mode & S_IWUSR){ //user write access
        strcat(result, "w");
        flag = true;
    }

    if(mode & S_IXUSR){ //user execute access
        strcat(result, "x");
        flag = true;
    }

    if(!flag)
        strcat(result, "-"); //if no permissions were identified...
}

//-------------------------

/**
 * return:
 * 1 --> regular file
 * 2 --> directory
 * 0 --> other (no output) 
 */
int checkFileType(char* name){

    struct stat stat_entry;

    if(lstat(name, &stat_entry) == -1){
        perror("Stat error");
        exit(3);
    }

    if(S_ISREG(stat_entry.st_mode))
        return 1;

    if(S_ISDIR(stat_entry.st_mode))
        return 2;

    return 0;
}

//-------------------------

void logLineAux(int fdLog, char* str) {

    struct timeval currentTime;
    if(gettimeofday(&currentTime, NULL) != 0) {  //extracts the current time
        fprintf(stderr, "Time extraction failed!");
        exit(1);
    }

    double time_taken; 
  
    //gets the time in milisseconds
    time_taken = (currentTime.tv_sec - startTime.tv_sec) * 1e6; 
    time_taken = (time_taken + (currentTime.tv_usec - startTime.tv_usec)) * 1e-3;

    sprintf(str, "%.2f - %8d - ", time_taken, getpid());
}

//-------------------------

void addCommandToLog(int fdLog, char* argv[], int argc) {

    char str[MAX_SIZE];
    logLineAux(fdLog, str);

    strcat(str, "COMMAND");

    for(int i = 0; i < argc; i++) {
        strcat(str, " ");
        strcat(str, argv[i]);
    }

    strcat(str, "\n");
    write(fdLog, str, strlen(str));
}

//-------------------------

void addFileToLog(int fdLog, char* fileName) {

    char str[MAX_SIZE];
    logLineAux(fdLog, str);

    strcat(str, "ANALYZED ");
    strcat(str, fileName);
    strcat(str, "\n");
    write(fdLog, str, strlen(str));
}

//-------------------------

void addDirToLog(int fdLog, char* dirName) {
    
    char str[MAX_SIZE];
    logLineAux(fdLog, str);

    strcat(str, "ENTERED ");
    strcat(str, dirName);
    strcat(str, "\n");
    write(fdLog, str, strlen(str));
}

//-------------------------

void addSignalToLog(int fdLog, char* sigName) {

    char str[MAX_SIZE];
    logLineAux(fdLog, str);

    strcat(str, "SIGNAL ");
    strcat(str, sigName);
    strcat(str, "\n");
    write(fdLog, str, strlen(str));
}

//-------------------------

//when the user presses CTRL+C
void sigintHandler(int signo) {

    if(dir != NULL)
        if(closedir(dir) != 0) {
        perror("closedir");
        exit(4);
    }

    if(boolArray[OUT])
        if(close(fdOut) == -1){
            perror("close");
            exit(5);
        }

    if(boolArray[LOG])
        if(close(fdLog) == -1){
            perror("close");
            exit(5);
        }

    exit(7); //exit code for when the program exits because of SIGINT
}

//-------------------------

void blocksigint() {

    sigset_t mask;
    if(sigemptyset(&mask) != 0) {
        perror("sigemptyset");
        exit(6);
    }
    if(sigaddset(&mask, SIGINT) != 0) {
        perror("sigaddset");
        exit(6);
    }
    if(sigprocmask(SIG_BLOCK, &mask, NULL) != 0) {
        perror("sigprocmask");
        exit(6);
    }
}

//-------------------------

void unblocksigint() {

    sigset_t mask;
    if(sigemptyset(&mask) != 0) {
        perror("sigemptyset");
        exit(6);
    }
    if(sigaddset(&mask, SIGINT) != 0) {
        perror("sigaddset");
        exit(6);
    }
    if(sigprocmask(SIG_UNBLOCK, &mask, NULL) != 0) {
        perror("sigprocmask");
        exit(6);
    }
}

//-------------------------

void equipHandlers() {

    struct sigaction action;
    action.sa_handler = sigintHandler;
    if(sigemptyset(&action.sa_mask) != 0) {
        perror("sigemptyset");
        exit(6);
    }
    action.sa_flags = 0;

    if(sigaction(SIGINT, &action, NULL) != 0) {
        perror("sigaction");
        exit(6);
    }

    //depois, acrescentar os outros handlers
}