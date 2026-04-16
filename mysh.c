#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "mysh.h"

static int read_line(int fd, char *buf, int maxlen) {
    int total = 0; 
    char c;

    while (total < maxlen - 1) {
        int n = read(fd, &c, 1);
        if (n <= 0) return n;
        buf[total++] = c;
        if (c == '\n') break;
    }

    buf[total] = '\0'; 
    return total;
}

static void print_prompt(void) {
    char cwd[BUF_SIZE]; 
    const char *home = getenv("HOME");

    if (!getcwd(cwd, sizeof(cwd))) return;

    size_t hlen = home ? strlen(home) : 0;
    if (home && strncmp(cwd, home, hlen) == 0 && (cwd[hlen] == '\0' || cwd[hlen] == '/')) {
        write(STDOUT_FILENO, "~", 1); 
        write(STDOUT_FILENO, cwd + hlen, strlen(cwd + hlen));
    } else { 
        write(STDOUT_FILENO, cwd, strlen(cwd)); 
    }

    write(STDOUT_FILENO, "$ ", 2);
}

int is_builtin(const char *name) {
    if (!name) return 0;
    return strcmp(name, "cd") == 0 || strcmp(name, "pwd") == 0 ||
           strcmp(name, "which") == 0 || strcmp(name, "exit") == 0;
}

int run_builtin(Command *cmd, int *last_status) {
    char *name = cmd->argv[0];

    if (strcmp(name, "exit") == 0) exit(0);

    if (strcmp(name, "pwd") == 0) {
        char cwd[BUF_SIZE];
        if (getcwd(cwd, sizeof(cwd))) {
            write(STDOUT_FILENO, cwd, strlen(cwd)); 
            write(STDOUT_FILENO, "\n", 1);
            return 0;
        }
        return 1;
    }

    if (strcmp(name, "cd") == 0) {
        char *target = cmd->argv[1] ? cmd->argv[1] : getenv("HOME");
        if (cmd->argv[1] && cmd->argv[2]) return 1;
        if (chdir(target) < 0) { perror("cd"); return 1; }
        return 0;
    }

    if (strcmp(name, "which") == 0) {
        char path[BUF_SIZE];
        if (!cmd->argv[1] || cmd->argv[2]) return 1;
        if (is_builtin(cmd->argv[1])) return 1;
        if (find_program(cmd->argv[1], path, sizeof(path))) {
            write(STDOUT_FILENO, path, strlen(path)); 
            write(STDOUT_FILENO, "\n", 1);
            return 0;
        }
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int in_fd = STDIN_FILENO, interactive = 0;

    if (argc == 2) {
        in_fd = open(argv[1], O_RDONLY);
        if (in_fd < 0) { perror(argv[1]); return 1; }
    } else { 
        interactive = isatty(STDIN_FILENO); 
    }

    if (interactive) {
        signal(SIGINT, SIG_IGN);
        write(STDOUT_FILENO, "Welcome to my shell!\n", 21);
    }

    char buf[BUF_SIZE]; 
    char *tokens[BUF_SIZE / 2]; 
    int last_status = 0;

    while (1) {
        if (interactive) {
            if (last_status > 0 && last_status < 128) {
                char msg[32]; 
                int len = snprintf(msg, sizeof(msg), "Exited with status %d\n", last_status);
                write(STDOUT_FILENO, msg, len);
            }
            print_prompt();
        }

        if (read_line(in_fd, buf, BUF_SIZE) <= 0) break;

        int ntok = tokenize(buf, tokens, BUF_SIZE / 2);
        if (ntok == 0) continue;

        Pipeline pl; 
        parse(tokens, ntok, &pl);
        last_status = execute(&pl, interactive);
    }

    if (interactive) write(STDOUT_FILENO, "Exiting my shell.\n", 18);
    return 0;
}
