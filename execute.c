#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h> 
#include <assert.h>

#include "execute.h"
#include "command.h"
#include "builtin.h"

#include "tests/syscall_mock.h"

static void args_destroy(char **args, unsigned int n) {
    for(unsigned int i = 0u; i < n; i++) {
        free(args[i]);
    }
    free(args);
}

static void die(const char* msj) {
    fprintf(stderr, "%s: %s\n", msj, strerror(errno));
    exit(EXIT_FAILURE);
}

static char ** scommand_to_args(scommand cmd, unsigned int length) {
    if(scommand_is_empty(cmd) || length == 0) {
        die("(scommand_to_args): invalid command");
    }
    char **args = malloc((length+1) * sizeof(char *));
    if(args == NULL) {
        die("(scommand_to_args): Error allocating memory");
    }
    for(unsigned int i = 0; i < length; i++) {
        char* arg = scommand_front(cmd);
        if(arg == NULL) {
            args_destroy(args, i); 
            die("(scommand_to_args): Invalid command arguments");
        }
        args[i] = strdup(arg);
        if(args[i] == NULL) {
            args_destroy(args, i);
            die("(scommand_to_args): Error with strdup");
        }
        scommand_pop_front(cmd);
    }
    args[length] = NULL;

    return args;
}

static void make_redirs(scommand cmd) {
    char *r_in = scommand_get_redir_in(cmd);
    char *r_out = scommand_get_redir_out(cmd);
    if(r_in == NULL && r_out == NULL) return;
    if(r_in != NULL) {
        int fd = open(r_in, O_RDONLY, 0666);
        if(fd < 0) {
            die("(make_redirs): Couldnt open the stdin file");
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            close(fd);
            die("(make_redirs): Error duplicating file descriptor to stdin");
        }
        close(fd);
    }
    if(r_out != NULL) {
        int fd = open(r_out, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if(fd < 0) {
            die("(make_redirs): Couldnt open the stdout file");
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            close(fd);
            die("(make_redirs): Error duplicating file descriptor to stdout");
        }
        close(fd);
    }
}

static void make_run_builtin(scommand cmd) {
    int saved_stdin = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdin < 0 || saved_stdout < 0) { 
        die("(make_run_builtin): Error duplicating file descriptor");
    }
    make_redirs(cmd);
    builtin_run(cmd);
    if (dup2(saved_stdin,  STDIN_FILENO)  < 0) die("(make_run_builtin): dup2 stdin");
    if (dup2(saved_stdout, STDOUT_FILENO) < 0) die("(make_run_builtin): dup2 stdout");
    close(saved_stdin);
    close(saved_stdout);
}

static void make_run_command(scommand cmd) {
    if(builtin_is_internal(cmd)) {
        make_run_builtin(cmd);
        exit(EXIT_SUCCESS);
    } else {
        unsigned int length = scommand_length(cmd);
        make_redirs(cmd);
        char **args = scommand_to_args(cmd, length);
        execvp(args[0], args);
        args_destroy(args, length);
        die("(make_run_command): Couldnt execute the command");
    }
}

void execute_pipeline(pipeline apipe) {
    assert(apipe != NULL);
    unsigned int apipe_length = pipeline_length(apipe);

    if(apipe_length == 1 ) {
        scommand cmd = pipeline_front(apipe);
        if (builtin_is_internal(cmd)) {
            make_run_builtin(cmd);
        } else {
            pid_t pid = fork();
            if(pid < 0) {
            die("(execute_pipeline): Not able to fork the process");
            }
            if(pid == 0) {
                make_run_command(cmd);
            }
            if(pipeline_get_wait(apipe)) {
                if(waitpid(pid, NULL, 0) < 0) {
                    die("(execute_pipeline): Couldnt wait for child process to complete");
                }
            }
        }
    } else {
        int (*fd)[2] = NULL;
        fd = malloc((apipe_length) * sizeof(*fd));
        pid_t *pids = malloc(apipe_length * sizeof(*pids));
        for(unsigned int i = 0; i < apipe_length; i++) {
            scommand cmd = pipeline_front(apipe);
            
            if(i < (apipe_length-1)) {
                if (pipe(fd[i]) == -1) die("Pipe couldnt be created");
            }
            if(builtin_is_internal(cmd)) {
                if(i != 0) {
                    if(dup2(fd[(i-1)][0], STDIN_FILENO) == -1) die("(execute_pipeline): error with dup stdin");
                }
                if(i != (apipe_length-1)) {
                    if(dup2(fd[i][1], STDOUT_FILENO) == -1)die("(execute_pipeline): error with dup stdout");
                }
                unsigned int current = (i < apipe_length-1) ? i+1 : i;
                for(unsigned int j = 0; j < current; j++) {
                    close(fd[j][0]);
                    close(fd[j][1]);
                }
                make_run_builtin(cmd);
                apipe_length--;
            }
            else {

                pid_t pid = fork();
                if(pid < 0) {
                    die("(execute_pipeline): Not able to fork the process");
                }
                if(pid == 0) {
                    if(i != 0) {
                        if(dup2(fd[(i-1)][0], STDIN_FILENO) == -1) die("(execute_pipeline): error with dup stdin");
                    }
                    if(i != (apipe_length-1)) {
                        if(dup2(fd[i][1], STDOUT_FILENO) == -1)die("(execute_pipeline): error with dup stdout");
                    }
                    unsigned int current = (i < apipe_length-1) ? i+1 : i;
                    for(unsigned int j = 0; j < current; j++) {
                        close(fd[j][0]);
                        close(fd[j][1]);
                    }
                    make_run_command(cmd);
                }
                pids[i] = pid;
            }
            pipeline_pop_front(apipe);
            if (i < apipe_length-1) close(fd[i][1]);
            if (i > 0) close(fd[i-1][0]);
        }
        if(pipeline_get_wait(apipe)) {
            for(unsigned int i = 0; i < apipe_length; i++) {
                if(waitpid(pids[i], NULL, 0) < 0) {
                    die("(execute_pipeline): Couldnt wait for child process to complete");
                }
            }
        }
    }
}
    





