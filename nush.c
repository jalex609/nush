#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tokenize.h"
#include "svec.h"

//names of implemented bulit-in functions 
char* builtIn[] = {
    "exit",
    "cd"
};

//creates the correct arrays for redirecting by removing < and >
char** make_redirect_arrays(int redirectIndex, svec* commands, int in ) {
        //redirect input operator
        if ( in == 0) {
            char ** args = malloc(commands->size * sizeof(char*));
            int ii = 0;
            for (ii; ii < commands->size; ++ii) {
                if (ii == redirectIndex) {
                    args[ii] = svec_get(commands, ii + 1);
                } else if (ii < commands->size && ii < redirectIndex) {
                    args[ii] = svec_get(commands, ii);
                } else if (ii < commands->size - 1 && ii > redirectIndex) {
                    args[ii] = svec_get(commands, ii + 1);
                } else {
                    args[ii] = 0;
                }
            }
            return args;
        }

        //redirect output operator
        if (in == 1) {

            char ** argsOuput = malloc((redirectIndex + 1) * sizeof(char*));
            int jj = 0;
            for (jj; jj < redirectIndex + 1; ++jj) {
                if (jj < redirectIndex) {
                    argsOuput[jj] = svec_get(commands, jj);
                } else {
                    argsOuput[jj] = 0;
                }
            }
            return argsOuput;
        }
    }

//puts a process in the background 
void doBackground(int backgroundIndex, svec* commands) {
    int cpid;

    if ((cpid = fork())) {
        int status;
        waitpid(cpid, & status, WNOHANG);
    } else {
        close(0);
        int bfd = open("dev/null", O_CREAT | O_TRUNC | O_WRONLY, S_IRWXG | S_IRWXO | S_IRWXU); //creates a space to run process in background
        dup(bfd);
        close(bfd);
        int ii = 0;
        commands->data[commands->size - 1] = 0;
        execvp(svec_get(commands, 0), commands->data);
    }
}

//Implements the functionality of the pipe operator in the shell
int
doPipe(int pipeIndex, svec* commands) {

    int cpid;
    if ((cpid = fork())) { //parent
        int status;
        waitpid(cpid, & status, 0);
    } else { //1st child process/parent of other two forks

        svec* left = make_svec();
        int ii = 0;
        for (ii; ii < pipeIndex; ++ii) {
            svec_push_back(left, svec_get(commands, ii));
        }
        svec* right = make_svec();
        int jj = 0;
        for (jj; jj < commands->size - pipeIndex - 1; ++jj) {
            svec_push_back(right, svec_get(commands, pipeIndex + jj + 1));
        }
        //set up pipe
        int pipes_fds[2];
        //0 for reading
        //1 for writing
        pipe(pipes_fds);

        if ((fork() == 0)) { //1st child of child of 1st fork
        
            close(pipes_fds[0]); //closing pipe read                      
            dup2(pipes_fds[1], 1); //replacing stdout with write end
            close(pipes_fds[1]);
            execute(left, 1);
            exit(0);
        } else if ((fork() == 0)) { //2nd child
        
            close(pipes_fds[1]); //closing pipe write         
            dup2(pipes_fds[0], 0); //replacing stdin with read end
            close(pipes_fds[0]);
            execute(right, 1); //recursively execute
            free_svec(right);
            exit(0);
        }

        close(pipes_fds[0]);
        close(pipes_fds[1]);
        wait(0); //wait for 1st child 
        free_svec(left);
        wait(0); //wait for 2nd child
        free_svec(right);

        exit(0);
    }

    return 1;
}

//implements the functionality of the semicolon operator for a shell
int doSemicolon(svec* cmd, int semiColonIndex) {
    svec* left = make_svec();
    int ii = 0;
    for (ii; ii < semiColonIndex; ++ii) {
        svec_push_back(left, svec_get(cmd, ii));
    }
    svec* right = make_svec();
    int jj = 0;
    for (jj; jj < cmd->size - semiColonIndex - 1; ++jj) {
        svec_push_back(right, svec_get(cmd, semiColonIndex + jj + 1));
    }

    execute(left, 0);
    free_svec(left);
    execute(right, 0);
    free_svec(right);
    return 1;
}

//implements the && and || operator for the shell
int doLogicalOperators(svec* cmd, int operatorIndex, int and) {
    svec* right = make_svec();
    int jj = 0;
    for (jj; jj < cmd->size - operatorIndex - 1; ++jj) {
        svec_push_back(right, svec_get(cmd, operatorIndex + jj + 1));
    }

    int ii = 0;
    char* leftArray[operatorIndex + 1]; 
    for (ii; ii < operatorIndex + 1; ++ii) {
        if (ii < operatorIndex) {
            leftArray[ii] = svec_get(cmd, ii);
        } else if (ii == operatorIndex) {
            leftArray[ii] = 0;
        }
    }

    int cpid;

    if ((cpid = fork())) {
        int status;
        if (waitpid(cpid, & status, 0) == -1) {
            perror("waitpid failed");
            return EXIT_FAILURE;
        }
        if (WIFEXITED(status)) {
            const int es = WEXITSTATUS(status); //checks exit status of call in child
            //and operator
            if (es == 0 && and == 1) {
                execute(right, 0);
            //or operator
            } else if (es != 0 && and == 0) {
                execute(right, 0);
            } else {
                return 1;
            }
        }
    } else {
        execvp(svec_get(cmd, 0), leftArray);
        return EXIT_FAILURE;
    }
    free_svec(right);
    return 1;
}

//checks whether an svec contains a certain string
int inputContains(svec* tokens, char* operator) {
    int ii = 0;
    for (ii; ii < tokens->size; ++ii) {
        if (strcmp(svec_get(tokens, ii), operator) == 0) {
            return ii;
        }
    }
    return -1;
}

//executes a line on the shell
int
execute(svec* cmd, int pipeFlag) {

    //empty arguemnt
    if (cmd->data[0] == NULL) {
        return 1;
    }

    //implements exit functionality
    if (strcmp(svec_get(cmd, 0), builtIn[0]) == 0) {
        exit(EXIT_SUCCESS);
    }

    int semiColonIndex = inputContains(cmd, ";");

    if (semiColonIndex != -1) {
        doSemicolon(cmd, semiColonIndex);
        return 1;
    }

    //piping
    int pipeIndex = inputContains(cmd, "|");

    if (pipeIndex != -1) {
        doPipe(pipeIndex, cmd);
        return 1;
    }

    //helper flag to make sure execute does not fork an extra time
    if (pipeFlag == 1) {
        char* args[cmd->size + 1];
        int ii = 0;
        for (ii; ii < cmd->size + 1; ++ii) {
            if (ii < cmd->size) {
                args[ii] = svec_get(cmd, ii);
            } else {
                args[ii] = 0;
            }
        }
        execvp(svec_get(cmd, 0), args);
        exit(0);
    }

    //and operator
    int andIndex = inputContains(cmd, "&&");

    if (andIndex != -1) {
        doLogicalOperators(cmd, andIndex, 1);
        return 1;
    }

    int orIndex = inputContains(cmd, "||");
    //or operator
    if (orIndex != -1) {
        doLogicalOperators(cmd, orIndex, 0);
        return 1;
    }

    int backgroundIndex = inputContains(cmd, "&");

    //checks background
    if (backgroundIndex != -1) {
        doBackground(backgroundIndex, cmd);
        return 1;
    }

    //changes the directory
    if (strcmp(svec_get(cmd, 0), builtIn[1]) == 0) {
        chdir(svec_get(cmd, 1));
        return 1;
    }

    //execvp if there are no operators or builtns
    int cpid;
    if ((cpid = fork())) {
        int status;
        waitpid(cpid, & status, 0);
    } else {

        //input redirect
        int redirectIndex = inputContains(cmd, "<");
        int outputRedirectIndex = inputContains(cmd, ">");

        if (outputRedirectIndex != -1) {
            int ofd = open(svec_get(cmd, outputRedirectIndex + 1), O_CREAT | O_TRUNC | O_WRONLY, S_IRWXG | S_IRWXO | S_IRWXU);
            dup2(ofd, 1);
            close(ofd);
            char ** args = make_redirect_arrays(outputRedirectIndex, cmd, 1);
            execvp(svec_get(cmd, 0), args);
            free(args);
            return 1;
        } else if (redirectIndex != -1) {
            int ifd = open(svec_get(cmd, redirectIndex + 1), O_RDONLY);
            dup2(ifd, 0);
            close(ifd);
            char ** args = make_redirect_arrays(redirectIndex, cmd, 0);
            execvp(svec_get(cmd, 0), args);
            free(args);

            return 1;
        } else {
            char* args[cmd->size + 1];
            int ii = 0;
            for (ii; ii < cmd->size + 1; ++ii) {

                if (ii < cmd->size) {
                    args[ii] = svec_get(cmd, ii);
                } else {
                    args[ii] = 0;
                }
            }
            execvp(svec_get(cmd, 0), args);

        }

    }
    return 1;
}

int
main(int argc, char* argv[]) {
    char cmd[1000];
    int keepGoing = 1;

    if (argc > 1) {
        fflush(stdout);
        FILE* scriptFile = fopen(argv[1], "r");
        while (fgets(cmd, 1000, scriptFile) != 0) {
            svec* readCmd = tokenize(cmd);
            execute(readCmd, 0);
            free_svec(readCmd);
        }
        return EXIT_SUCCESS;
    }

    do {

        printf("nush$ ");
        char* inputLine = fgets(cmd, 1000, stdin);

        if (!inputLine) {
            break;
        }

        svec* realcmd = tokenize(cmd);

        keepGoing = execute(realcmd, 0);

        free_svec(realcmd);

    } while (keepGoing);

    return EXIT_SUCCESS;
}