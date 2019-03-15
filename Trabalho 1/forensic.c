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

#define REC         0
#define MD5         1
#define SHA1        2
#define SHA256      3
#define OUT         4
#define LOG         5
#define NUM_OPTIONS 6

void incUsage(){
    printf("Illegal use/number of arguments\n");
    printf("Usage: forensic [-r] [-h [md5[,sha1[,sha256]]] [-o <outfile>] [-v] <file|dir>\n");
    exit(1);
}


void analiseFile(char* file) {

    struct stat stat_entry;

    if(lstat(file, &stat_entry) == -1){
        perror("Stat error");
        exit(1);
    }

    if(S_ISREG(stat_entry.st_mode)){ //if regular file

        
    }
}

//-------------------------

void extractHOptions(bool array[], char* string) {

    if(strcmp(string, "md5") == 0){
        array[MD5] = true;
        return;
    }
    else if(strcmp(string, "sha1") == 0){
        array[SHA1] = true;
        return;
    }
    else if(strcmp(string, "sha256") == 0){
        array[SHA256] = true;
        return;
    }

    char comma[2] = ",";
    char* token;

    token = strtok(string, comma);

    while(token != NULL){

        //tries to extract the different options that are available for -h

        if(strcmp(token, "md5") == 0)
            array[MD5] = true;
        else if(strcmp(token, "sha1") == 0)
            array[SHA1] = true;
        else if(strcmp(token, "sha256") == 0)
            array[SHA256] = true;
        else
            incUsage();

        token = strtok(NULL, comma);
    }

    if(!(array[MD5] || array[SHA1] || array[SHA256]))
        incUsage();
}

//-------------------------

void extractOptions(bool array[], int argc, char* argv[], int *fdOut, int *fdLog) {

    if(argc == 1) 
        incUsage(); //if no arguments are specified

    if((!strcmp(argv[argc - 1], "-r")) || (!strcmp(argv[argc - 1], "-v")) || (!strcmp(argv[argc - 1], "-o")) || (!strcmp(argv[argc - 1], "-h")))
        incUsage(); //if last argument is a flag
        

    for(int i = 0; i < NUM_OPTIONS; i++)
        array[i] = false;


    bool filePresent = false; //in order to point out that the file/directory was specified in the command call
    char* fileOut, *fileLog; //names of the files to be used for output and log (if any)

    for(int k = 1; k < argc; k++){
                
        if(strcmp(argv[k], "-r") == 0){ //recursive option
            
            if(array[REC])
                incUsage(); //if it was already activated
            
            array[REC] = true;
    
        }
        else if(strcmp(argv[k], "-h") == 0){ //md5, sha1 and/or sha256 option

            if(array[MD5] || array[SHA1] || array[SHA256])
                incUsage(); //if it was already activated

            extractHOptions(array, argv[k + 1]);
            k++; //skips the next argument, which is part of the -h flag specification

        }
        else if(strcmp(argv[k], "-o") == 0){ //output option

            if(array[OUT])
                incUsage(); //if it was already activated

            array[OUT] = true;

            if((!strcmp(argv[k + 1], "-r")) || (!strcmp(argv[k + 1], "-v")) || (!strcmp(argv[k + 1], "-o")) || (!strcmp(argv[k + 1], "-h")))
                incUsage(); //if next argument is a flag
        
            fileOut = argv[k + 1]; //saves the file name
            k++; //skips the next argument, because it is the name of the output file
        
        }
        else if(strcmp(argv[k], "-v") == 0){ //log file option
            
            if(array[LOG])
                incUsage(); //if it was already activated

            array[LOG] = true;

            if(getenv("LOGFILENAME") == NULL){
                perror("LOGFILENAME");
                exit(2);
            }

            fileLog = getenv("LOGFILENAME"); //saves the file name

        }
        else if(k != argc - 1) //if it is not a flag, and if it is not the last argument, invalid option
            incUsage();
        
        else filePresent = true; //last argument; its the name of the file 
        
    }

    if(!filePresent)
        incUsage();

    if(array[OUT])
        if((*fdOut = open(fileOut, O_WRONLY | O_CREAT, 0644)) == -1){
            perror(fileOut);
            exit(2);
        }

    if(array[LOG])
        if((*fdLog = open(fileLog, O_WRONLY | O_CREAT | O_APPEND, 0644)) == -1){
            perror(fileLog);
            exit(2);
        }
}

//-------------------------

int main(int argc, char* argv[], char* envp[]) {

    bool boolArray[NUM_OPTIONS]; //array to save the different program options
    int fdOut, fdLog;

    extractOptions(boolArray, argc, argv, &fdOut, &fdLog);

    char* inputName = argv[argc - 1]; //name of the file or directory

    printf("REC: ");
    if(boolArray[REC])
        printf("yes\n");
    else printf("no\n");

    printf("MD5: ");
    if(boolArray[MD5])
        printf("yes\n");
    else printf("no\n");

    printf("SHA1: ");
    if(boolArray[SHA1])
        printf("yes\n");
    else printf("no\n");

    printf("SHA256: ");
    if(boolArray[SHA256])
        printf("yes\n");
    else printf("no\n");

    printf("OUT: ");
    if(boolArray[OUT])
        printf("yes, file descriptor %d\n", fdOut);
    else printf("no\n");

    printf("LOG: ");
    if(boolArray[LOG])
        printf("yes, file name %s\n", getenv("LOGFILENAME"));
    else printf("no\n");

    printf("File name: %s\n\n", inputName);

    return 0;
}
