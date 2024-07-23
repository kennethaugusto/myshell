#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc == 1) {
        if (isInteractive(stdin)) {
            printf("Welcome to my shell!\n");
            interactiveMode();
            printf("Exiting my shell.\n");
        }
        else {
            batchMode(NULL);
        }
    } else if (argc == 2) {
        batchMode(argv[1]);
    } else {
        fprintf(stderr, "Usage: %s [script file]\n", argv[0]);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int isInteractive(FILE *stream) {
    return isatty(fileno(stream));
}

void interactiveMode() {
    char *line = NULL;
    size_t bufsize = 0;
    ssize_t linelen;
    char **args;
    int status = 1;

    while (status) {
        printf("mysh> ");
        linelen = getline(&line, &bufsize, stdin);

        if (linelen == -1) {
            if (feof(stdin)) {  // End of file (Ctrl-D)
                printf("\nExiting my shell.\n");
                break;
            } else {  // Error
                perror("getline");
                continue;
            }
        }

        args = parseCommand(line);
        status = executeCommand(args);

        free(line);
        line = NULL;
        free(args);
    }
}

void batchMode(const char *filename) {
    FILE *fp = filename ? fopen(filename, "r") : stdin;
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char *line = NULL;
    size_t bufsize = 0;
    ssize_t linelen;
    char **args;
    int status = 1;

    while (status && (linelen = getline(&line, &bufsize, fp)) != -1) {
        args = parseCommand(line);
        status = executeCommand(args);

        free(line);
        line = NULL;
        free(args);
    }

    if (filename) {
        fclose(fp);
    }

    if (line) {
        free(line);
    }
}

char** parseCommand(char* line) {
    int bufsize = 64, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, " \t\r\n\a");
    while (token != NULL) {
        tokens[position++] = token;

        if (position >= bufsize) {
            bufsize += 64;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, " \t\r\n\a");
    }
    tokens[position] = NULL; // Null-terminate the list of arguments
    return tokens;
}

int launchExternalCommand(char **args) {
    pid_t pid, wpid;
    int status;

    pid = fork();
    if (pid == 0) {
        // Child process
        if (execvp(args[0], args) == -1) {
            perror("execvp error");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Error forking
        perror("forking error");
    } else {
        // Parent process
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1; // Indicate to continue execution
}

int executeCommand(char **args) {
    if (args[0] == NULL) {
        // An empty command was entered
        return 1;
    }

    if (strcmp(args[0], "exit") == 0) {
        // Exit command
        return 0; // Return 0 to indicate shell should terminate
    }

    // Additional built-in commands can be added here

    return launchExternalCommand(args); // For commands not built into the shell
}