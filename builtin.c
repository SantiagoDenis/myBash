
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include "builtin.h"
#include "command.h"
#include "tests/syscall_mock.h"


bool builtin_is_internal(scommand cmd){
    assert(cmd != NULL);
    char* verify = scommand_front(cmd);
    return (strcmp(verify, "cd") == 0 || strcmp(verify, "help") == 0 || strcmp(verify, "exit") == 0);
}

bool builtin_alone(pipeline p) {
    assert(p != NULL);
    return (pipeline_length(p) == 1 && pipeline_front(p) != NULL && builtin_is_internal(pipeline_front(p)));
}

void builtin_run(scommand cmd){
    assert(builtin_is_internal(cmd));
    char *internal = scommand_front(cmd);
    if (strcmp(internal, "cd") == 0){
        scommand_pop_front(cmd);
        if (scommand_is_empty(cmd)){
            perror("Couldnt find the directory");
        } else {
            char *path = scommand_front(cmd);
            if (chdir(path) != 0){
                perror("cd");
                return;
            }
        }
    } else if (strcmp(internal, "help") == 0){
        printf("Mybash: Guadalupe Acosta, Santiago Denis, Carla Diaz, Emil Morano\n\n");
        printf(" Supported internal commands:\n");
        printf("   cd <directory> : Changes the directory\n");
        printf("   help : Prints this message\n");
        printf("   exit : Close the shell\n");
        
    } else if (strcmp(internal, "exit") == 0){
       scommand_pop_front(cmd);
        exit(0);
    }
}

