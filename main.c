#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>

#include "global.h"

// This is the file that you should work on.

// declaration
int execute (struct cmd *cmd, bool is_toplevel);

// name of the program, to be printed in several places
#define NAME "myshell"

// Some helpful functions

void errmsg (char *msg) {
    fprintf(stderr,"error: %s\n",msg);
}

bool interrupted = false;
// signal handler so that SIGINT does not kill main
void stay_on_ctrlc () {
    signal(SIGINT, stay_on_ctrlc);
    printf("\nSIGINT\n");
    interrupted = true;
}

// signal handler so that SIGINT kills children
void exit_on_ctrlc () {
    exit(131);
}

// return code from string for exit function
int retcode(char* arg) {
    int total = 0;
    if (arg == NULL) return 0;
    for (char* c = arg; *c != '\0' && '0' <= *c && *c <= '9'; c++) {
        total = total * 10 + *c - '0';
    }
    return total;
}


// apply_redirects() should modify the file descriptors for standard
// input/output/error (0/1/2) of the current process to the files
// whose names are given in cmd->input/output/error.
// append is like output but the file should be extended rather
// than overwritten.
void apply_redirects (struct cmd *cmd) {
    if (cmd->output) { dup2(open(cmd->output, O_WRONLY | O_CREAT | O_TRUNC, 0644), STDOUT_FILENO); }
    if (cmd->error) { dup2(open(cmd->error, O_WRONLY | O_CREAT | O_TRUNC, 0644), STDERR_FILENO); }
    if (cmd->input) { dup2(open(cmd->input, O_RDONLY, 0444), STDIN_FILENO); }
    if (cmd->append) { dup2(open(cmd->append, O_APPEND | O_WRONLY | O_CREAT, 0644), STDOUT_FILENO); }
}

// The function execute() takes a command parsed at the command line.
// The structure of the command is explained in output.c.
// Returns the exit code of the command in question.
int execute (struct cmd* cmd, bool is_toplevel) {
    switch (cmd->type) {
        case C_PLAIN: {
            int cpid;
            // exit is a builtin, not an external command
            if (strcmp(cmd->args[0], "exit") == 0) {
                printf("goodbye.\n");
                exit(retcode(cmd->args[1]));
            }
            if ((cpid = fork())) {
                int status;
                signal(SIGINT, SIG_IGN);
                waitpid(cpid, &status, 0);
                if (WIFEXITED(status)) {
                    status = WEXITSTATUS(status);
                    if (is_toplevel && status != 0) {
                        printf("Exited with nonzero status %d\n", status);
                    }
                } else if (status == 2) {
                    // interrupt
                    printf("  SIGINT\n");
                    interrupted = true;
                    status = 130;
                } else {
                    // something wrong happened, assume it's because command is undefined
                    fprintf(stderr, "Unknown command '%s'\n", cmd->args[0]);
                    status = 255;
                }
                return status;
            } else {
                apply_redirects(cmd);
                signal(SIGINT, exit_on_ctrlc);
                execvp(cmd->args[0], cmd->args);
            }
        }
        case C_SEQ: {
            execute(cmd->left, false);
            if (interrupted) return 130;
            return execute(cmd->right, false);
        }
        case C_AND: {
            int status = execute(cmd->left, false);
            if (status) return status;
            if (interrupted) return 130;
            return execute(cmd->right, false);
        }
        case C_OR: {
            int status = execute(cmd->left, false);
            if (!status) return status;
            if (interrupted) return 130;
            return execute(cmd->right, false);
        }
        case C_PIPE: {
            int tube [2];
            pipe(tube);
            int pid1, pid2;
            if (!(pid1 = fork())) {
                // first child: execute left, pipe output
                signal(SIGINT, exit_on_ctrlc);
                close(tube[0]);
                dup2(tube[1], STDOUT_FILENO);
                close(tube[1]);
                int retcode = execute(cmd->left, false);
                exit(retcode);
            } else if (!(pid2 = fork())) {
                // second child: execute right, pipe input
                signal(SIGINT, exit_on_ctrlc);
                close(tube[1]);
                dup2(tube[0], STDIN_FILENO);
                close(tube[0]);
                int retcode = execute(cmd->right, false);
                exit(retcode);
            } else {
                // parent: close pipe and wait for children
                close(tube[0]); close(tube[1]);
                int status1, status2, status;
                waitpid(pid1, &status1, 0);
                waitpid(pid2, &status2, 0);
                if (WIFEXITED(status1)) {
                    if (WIFEXITED(status2)) {
                        status1 = WEXITSTATUS(status1);
                        status2 = WEXITSTATUS(status2);
                        if (status1) { status = status1; } else { status = status2; }
                        if (is_toplevel && status != 0) {
                            printf("Exited with nonzero status %d\n", status);
                        }
                    } else {
                        fprintf(stderr, "Unknown command '%s'\n", cmd->right->args[0]);
                        status = 255;
                    }
                } else {
                    fprintf(stderr, "Unknown command '%s'\n", cmd->left->args[0]);
                    status = 255;
                }
                return status;
            }
        }
        case C_VOID: {
            int cpid;
            if ((cpid = fork())) {
                int status;
                waitpid(cpid, &status, 0);
                if (WIFEXITED(status)) {
                    status = WEXITSTATUS(status);
                    if (is_toplevel && status != 0) {
                        printf("Exited with nonzero status %d\n", status);
                    }
                } else {
                    fprintf(stderr, "Some error occured\n");
                    status = 254;
                }
                return status;
            } else {
                // child
                apply_redirects(cmd);
                int status = execute(cmd->left, false);
                exit(status);
            }
        }
        return -1;
    }

    // Just to satisfy the compiler
    errmsg("This cannot happen!");
    return -1;
}

int main (int argc, char** argv) {
    char* prompt = malloc(strlen(NAME)+10);
    printf("welcome to %s!\n", NAME);
    sprintf(prompt,"(%d) %s> ", getpid(), NAME);

    bool loop = true;
    while (loop) {
        signal(SIGINT, stay_on_ctrlc);
        interrupted = false;
        char* line = readline(prompt);
        if (interrupted) continue; // user pressed Ctrl+C; wait for next command after the newline
        if (!line) break; // user pressed Ctrl+D; quit shell
        if (!*line) continue; // empty line
        add_history (line); // add line to history
        struct cmd *cmd = parser(line);
        if (!cmd) {
            // some parse error occurred; ignore
            printf("Parsing error\n");
            continue;
        }
        //output(cmd,0); // activate this for debugging
        execute(cmd, true);
    }

    printf("goodbye!\n");
    return 0;
}
