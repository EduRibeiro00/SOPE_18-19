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

#define MAX_SIZE    255

void commandToString(char* result, int max_size, char* command, char* file){

        FILE* fileOut;

        char pipeCommand[MAX_SIZE];
        strcpy(pipeCommand, command);
        strcat(pipeCommand, " ");
        strcat(pipeCommand, file);

        if((fileOut = popen(pipeCommand, "r")) == NULL){ //uses a pipe in order to get the output of the command
            printf("Popen error!\n");
            exit(3);
        }

        if(fgets(result, max_size, fileOut) == NULL){ //extracts the result of the command from the file pointer
            printf("Error with fgets!\n");
            exit(3);
        }

        result[strlen(result) - 1] = 0; //gets rid of the newline

        if(pclose(fileOut) != 0){
            printf("Pclose error!\n");
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