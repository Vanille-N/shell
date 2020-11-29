#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include "global.h"

// This is the file that you should work on.

// declaration
int execute (struct cmd *cmd);

// name of the program, to be printed in several places
#define NAME "myshell"

// Some helpful functions

void errmsg (char *msg)
{
	fprintf(stderr,"error: %s\n",msg);
}

// apply_redirects() should modify the file descriptors for standard
// input/output/error (0/1/2) of the current process to the files
// whose names are given in cmd->input/output/error.
// append is like output but the file should be extended rather
// than overwritten.

void apply_redirects (struct cmd *cmd)
{
	if (cmd->input || cmd->output || cmd->append || cmd->error)
	{
		errmsg("I do not know how to redirect, please help me!");
		exit(-1);
	}
}

// The function execute() takes a command parsed at the command line.
// The structure of the command is explained in output.c.
// Returns the exit code of the command in question.

int execute (struct cmd *cmd)
{
	switch (cmd->type)
	{
	    case C_PLAIN: {
            int cpid;
            if ((cpid = fork())) {
                int status;
                waitpid(cpid, &status, 0);
                if (WIFEXITED(status)) {
                    status = WEXITSTATUS(status);
                } else {
                    fprintf(stderr, "Unknown command '%s'\n", cmd->args[0]);
                    status = 255;
                }
                return status;
            } else {
                execvp(cmd->args[0], cmd->args);
            }
        }
	    case C_SEQ: {
            execute(cmd->left);
            return execute(cmd->right);
        }
	    case C_AND: {
            int status = execute(cmd->left);
            if (status) return status;
            return execute(cmd->right);
        }
	    case C_OR: {
            int status = execute(cmd->left);
            if (!status) return status;
            return execute(cmd->right);
        }
	    case C_PIPE: {
            int tube [2];
            pipe(tube);
            int pid1, pid2;
            if (!(pid1 = fork())) {
                // first child: execute left, pipe output
                close(tube[0]);
                dup2(tube[1], STDOUT_FILENO);
                close(tube[1]);
                int retcode = execute(cmd->left);
                exit(retcode);
            } else if (!(pid2 = fork())) {
                // second child: execute left, pipe input
                close(tube[1]);
                dup2(tube[0], STDIN_FILENO);
                close(tube[0]);
                int retcode = execute(cmd->right);
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
	    case C_VOID:
		errmsg("I do not know how to do this, please help me!");
		return -1;
	}

	// Just to satisfy the compiler
	errmsg("This cannot happen!");
	return -1;
}

int main (int argc, char **argv)
{
	char *prompt = malloc(strlen(NAME)+3);
	printf("welcome to %s!\n", NAME);
	sprintf(prompt,"%s> ", NAME);

	while (1)
	{
		char *line = readline(prompt);
		if (!line) break;	// user pressed Ctrl+D; quit shell
		if (!*line) continue;	// empty line

		add_history (line);	// add line to history

		struct cmd *cmd = parser(line);
		if (!cmd) continue;	// some parse error occurred; ignore
		//output(cmd,0);	// activate this for debugging
		execute(cmd);
	}

	printf("goodbye!\n");
	return 0;
}
