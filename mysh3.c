#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

char* readline();
char** parse(char* input);
int execute(char** args);
int makePipe(char** args1, char** args2);
//builtin functions
int quit(char** args);
int cd(char** args);
int clear(char** args);
int dir(char** args);
int environ(char** args);
int echo(char** args);
int help(char** args);

#define BUFSIZE  1024
#define NUMBUILTINS 7

char* builtins[] = {"quit", "cd", "clear", "dir", "environ", "echo", "help"};
int (*builtinFN[])(char **) = {&quit, &cd, &clear, &dir, &environ, &echo, &help};

/* main function: shell prompt loop */
int main(int argc, char* argv[]) {
    char input[BUFSIZE];
    char **inputArray;
    int status = 1, bytesRead;
    FILE *inputStream = stdin;

    if (argc > 1) {
        inputStream = fopen(argv[1], "r");
        if (!inputStream) {
            perror("Error opening file");
            exit(EXIT_FAILURE);
        }
    }
    
    if (isatty(fileno(inputStream))) {
        system("clear");
        printf("Welcome to my shell!\n");
    }

    while (status != 0) {
        if (isatty(fileno(inputStream))) {
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("mysh> ");
                fflush(stdout);
            }
        }
        bytesRead = read(fileno(inputStream), input, BUFSIZE - 1);
        if (bytesRead <= 0) {
            if (feof(inputStream) || bytesRead == 0) break;
            perror("read");
            continue;
        }
        input[bytesRead - 1] = '\0';
        inputArray = parse(input);
        status = execute(inputArray);
        free(inputArray);
    }

    if (argc > 1) {
        fclose(inputStream);
    }

    if (isatty(fileno(stdin))) {
        printf("Exiting my shell.\n");
        system("clear");
       }
    return 0;
}

char* readline() {
    char* input = malloc(sizeof(char) * BUFSIZE);
    int pos = 0;
    char c;
    while((c = getchar()) != '\n' && c != EOF) {
        input[pos] = c;
        pos++;
        if(pos >= BUFSIZE - 1) {
            printf("User input too long. Please input args less than 1024 characters.\n");
		    break;
        }
    }
    input[pos] = '\0';
    return input;
}

char** parse(char* input) {
    int i = 0;
    char** arrayArgs = malloc(sizeof(char) * BUFSIZE);
    const char* space = " ";
    char* arg = strtok(input, space);
    while(arg != NULL) {
        arrayArgs[i] = arg;
        i++;
        arg = strtok(NULL, space);
    }
    arrayArgs[i] = NULL;
    return arrayArgs;
}

int execute(char** args) {
    int ret = 1;
    int isBuiltin = 0;
    int isRedirectIn = 0;
    int isRedirectOut = 0;
    int inBg = 0;
    int i, j;
    int stdinDup, stdoutDup, infile, outfile;
    if (args[0] == NULL) {
        return ret;
    }
    i = 0;
    while(args[i] != NULL) {
        if(strcmp(args[i], "&") == 0) {
            inBg = 1;
        }
        else if(strcmp(args[i], "<") == 0) {
            infile = open(args[i+1], O_RDONLY);
            args[i] = NULL;
            isRedirectIn = 1;
        }
        else if(strcmp(args[i], ">") == 0) {
            outfile = open(args[i+1], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR | O_CLOEXEC);
            args[i] = NULL;
            isRedirectOut = 1;
        }
        else if(strcmp(args[i], ">>") == 0) {
            outfile = open(args[i+1], O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR | O_CLOEXEC);
            args[i] = NULL;
            isRedirectOut = 1;
        }
        else if(strcmp(args[i], "|") == 0) {
            args[i] = NULL;
            ret = makePipe(&args[0], &args[i+1]);
            return ret;
        }
        i++;
    }
    for (j = 0; j < NUMBUILTINS; j++) {
        if(strcmp(args[0], builtins[j]) == 0) { 
            stdinDup = dup(STDIN_FILENO);
            stdoutDup = dup(STDOUT_FILENO);
        if(isRedirectIn == 1) {
            dup2(infile, STDIN_FILENO);
        }
        if(isRedirectOut == 1) {
            dup2(outfile, STDOUT_FILENO);
        }
        ret = (builtinFN[j])(args);
        dup2(stdoutDup, STDOUT_FILENO);
        dup2(stdinDup, STDIN_FILENO);
        isBuiltin = 1;
        }
    }
    if(isBuiltin == 0) {
    //FORK to create child
        pid_t pid;
        pid = fork();
        if(pid < 0) {
            printf("Error forking\n");
        }
        else if(pid == 0) {
            if(isRedirectIn == 1) {
                dup2(infile, STDIN_FILENO);
            }
            if(isRedirectOut == 1) {
                dup2(outfile, STDOUT_FILENO);
            }
        execvp(args[0], args);
        printf("Command or executable not recognized.\n");
        }
    else {
        if(inBg == 0) {
            waitpid(pid, NULL, 0);
        }
        }
    }
    return ret;
}

int makePipe(char** args1, char** args2) {
    pid_t pid1, pid2;
    int fd[2];
    pipe(fd);
    pid1 = fork();
    if (pid1 < 0) {
        printf("Error forking.\n")
    }
    else if (pid1 == 0) {
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        execute(args1);
        exit(0);
    }
    else {
        pid2 = fork();
        if (pid2 < 0) {
            printf("Error forking.\n");
        }
        else if (pid2 == 0) {
            dup2(fd[0], STDIN_FILENO);
            close(fd[1]);
            execute(args2);
            exit(0);
        }
        else {
            close(fd[0]);
            close(fd[1]);
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);
        }
    }
    return 1;
}

int quit(char** args) {
    return 0;
}

int cd(char** args) {
    if (chdir(args[1]) != 0) {
        printf("No directory '%s' found.\n", args[1]);
    }
    return 1;
}

int clear(char** args) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int r;
    for (r = 0; r < w.ws_row; r++) {
        printf("\n");
    }
    return 1;
}

int dir(char** args) {
    DIR *dir;
    struct dirent *dirContents;
    if (args[1] == NULL) {
        dir = opendir("./");
    }
    else {
        if((dir = opendir(args[1])) == NULL) {
            printf("No such directory %s.\n", args[1]);
            return 1;
        }
    }
    while((dirContents = readdir(dir)) != NULL) {
        printf(" %s ", dirContents->d_name);
    }
    printf("\n");
    closedir(dir);
    return 1;
}

int environ(char** args) {
    int numVars = 7;
    char* vars[] = {"USER", "HOME", "PATH", "SHELL", "OSTYPE", "PWD", "GROUP"};
    int i;
    for (i = 0; i < numVars; i++) {
        const char* v = getenv(vars[i]);
        if (v != NULL) {
            printf("%s=%s\n", vars[i], v);
        }
        else {
            printf("Error: Could not find environment variable %s.\n", vars[i]);
        }
    }
    return 1;
}

int echo(char** args) {
    int i = 1;
    while(args[i] != NULL) {
        printf("%s ", args[i]);
        i++;
    }
    printf("\n");
    return 1;
}

int help(char** args) {
    FILE* fp = fopen("readme", "r");
    if (fp == NULL) {
        printf("Can't find readme file.\n");
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    int len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char text[len];
    if (isatty(1)) {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        int rows = w.ws_row - 1;  
        int r = 1;
        int lastpos = 0;
        int curpos;
        fgets(text, len, fp);
        do {
            printf("%s", text);
            curpos = ftell(fp);
            int len = curpos - lastpos;
            while(len > w.ws_col) {
                r++;
                len -= w.ws_col;
            }
        r++;
        lastpos = curpos;
        }
        while(fgets(text, len, fp) != NULL && r < rows);
        printf("%s", text);
        char* input;
        while(1) {
            input = readline();
            if(strcmp(input, "q") == 0) {
                return 1;
            }
            else {
                if (fgets(text, len, fp) == NULL) {
                  break;
                }
            else {
                strtok(text, "\n");
                printf("%s", text);
        }
      }
    }
    printf("Reached end of help file. Press enter to return to the shell.\n");\
    getchar();
    }
    else {
        while (fgets(text, len, fp) != NULL ) {
            printf("%s", text);
        }
    }
    return 1;
}