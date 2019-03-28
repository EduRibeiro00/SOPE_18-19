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

void incUsage(){
    printf("Illegal use/number of arguments\n");
    printf("Usage: forensic [-r] [-h [md5[,sha1[,sha256]]] [-o <outfile>] [-v] <file|dir>\n");
    exit(1);
}

//-------------------------

const char* firstCharAfterSpace(const char* input) {
   const char* starting = input;
   while (*starting != ' ') {
     starting++;
   }

   starting++;
   return starting;
 }
