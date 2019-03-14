#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <string.h> 

#define REC         0
#define MD5         1
#define SHA1        2
#define SHA256      3
#define OUT         4
#define LOG         5
#define NUM_OPTIONS 6


void analiseFile(char* file){

    struct stat stat_entry;

    if(lstat(file, &stat_entry) == -1){
        perror("Stat error");
        exit(1);
    }

    if(S_ISREG(stat_entry.st_mode)){ //if regular file

        //printf("%d,%d,%d,%d,%d,%d", )
    }
}

void extractOptions(bool array[], int size, int argc, char* argv[]){

    if(argc == 1){
        printf("Illegal number of arguments\n");
        printf("Usage: forensic [-r] [-h [md5[,sha1[,sha256]]] [-o <outfile>] [-v] <file|dir>\n");
        exit(1);
    }

    for(int i = 0; i < size; i++)
        array[i] = false;

    bool end = false; //in order to know when to stop the cycle (every argument read)

    for(int k = 1; k < argc; k++){
                
        if(strcmp(argv[k], "-r") == 0){
            array[REC] = true;
        }

        if(strcmp(argv[k], "-v") == 0){
            array[LOG] = true;
        }

        
    }
    
}


int main(int argc, char* argv[], char* envp[]){

    bool boolArray[NUM_OPTIONS]; //array to save the different program options
    

    char* inputName = argv[argc - 1]; //name of the file or directory

    exit(0);
}