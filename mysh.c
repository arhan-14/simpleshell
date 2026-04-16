#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#define BUF_SIZE 4096

static int read_line(int fd, char *buf, int maxlen) {
    int total = 0;
    char c;
    while (total < maxlen - 1) {
        int n = read(fd, &c, 1);
        if (n < 0) { perror("read"); return -1; }
        if (n == 0) break;
        buf[total++] = c;
        if (c == '\n') break;
    }
    buf[total] = '\0';
    return total;
}

int tokenize(char *line, char **tokens, int max_tokens) {
    int count = 0;
    char *p = line;
    while (*p && count < max_tokens - 1) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n' || *p == '#') break;
        tokens[count++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
        if (*p) *p++ = '\0';
    }
    tokens[count] = NULL;
    return count;
}

static void print_prompt(void) {
    char cwd[BUF_SIZE];
    const char *home = getenv("HOME");
    if (!getcwd(cwd, sizeof(cwd))) {
        write(STDOUT_FILENO, "$ ", 2);
        return;
    }
    size_t hlen = home ? strlen(home) : 0;
    if (home &&
        strncmp(cwd, home, hlen) == 0 &&
        (cwd[hlen] == '\0' || cwd[hlen] == '/')) {

        write(STDOUT_FILENO, "~", 1);
        write(STDOUT_FILENO, cwd + hlen, strlen(cwd + hlen));

    } else {
        write(STDOUT_FILENO, cwd, strlen(cwd));
    }
    write(STDOUT_FILENO, "$ ", 2);
}

int find_program(const char *name, char *out, size_t outlen) {
    const char *dirs[] = { "/usr/local/bin", "/usr/bin", "/bin", NULL };
    for (int i = 0; dirs[i]; i++) {
        snprintf(out, outlen, "%s/%s", dirs[i], name);
        if (access(out, X_OK) == 0) return 1;
    }
    return 0;
}

static int has_pipe(char **tokens, int ntok){
    for (int i = 0; i < ntok; i++) {
        if (tokens[i][0] == '|') return 1;
    }
    return 0;
}

static int run_pipeline(char **tokens, int ntok, int interactive) {

    if (tokens[0][0] == '|' || tokens[ntok - 1][0] == '|') {
        write(STDERR_FILENO, "syntax error\n", 13);
        return 1;
    }

    int seg_start[BUF_SIZE / 2];
    int seg_count = 0;
    seg_start[seg_count++] = 0;

    for (int i = 0; i < ntok; i++) {
        if (tokens[i][0] == '|') {
            tokens[i] = NULL;
            if (i + 1 < ntok)
                seg_start[seg_count++] = i + 1;
        }
    }

    for (int s = 0; s < seg_count; s++) {
        char **seg = tokens + seg_start[s];
        if (seg[0] == NULL) {
            write(STDERR_FILENO, "syntax error\n", 13);
            return 1;
        }
    }

    int num_pipes = seg_count - 1;
    int pipefds[BUF_SIZE / 2][2];

    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipefds[i]) < 0) {
            perror("pipe");
            for (int j = 0; j < i; j++) {
                close(pipefds[j][0]);
                close(pipefds[j][1]);
            }
            return 1;
        }
    }

    pid_t pids[BUF_SIZE / 2];
    int launched = 0;

    for (int s = 0; s < seg_count; s++) {
        char **seg = tokens + seg_start[s];

        int ci = 0;
        char *clean[BUF_SIZE / 2];
        for (int i = 0; seg[i] != NULL; i++) {
            if (!is_operator(seg[i]))
                clean[ci++] = seg[i];
        }
        clean[ci] = NULL;

        if (ci == 0) {
            write(STDERR_FILENO, "syntax error\n", 13);
            for (int i = 0; i < num_pipes; i++) {
                close(pipefds[i][0]);
                close(pipefds[i][1]);
            }
            for (int i = 0; i < launched; i++) waitpid(pids[i], NULL, 0);
            return 1;
        }

        char path[BUF_SIZE];
        if (strchr(clean[0], '/')) {
            snprintf(path, sizeof(path), "%s", clean[0]);
        } else if (!find_program(clean[0], path, sizeof(path))) {
            write(STDERR_FILENO, clean[0], strlen(clean[0]));
            write(STDERR_FILENO, ": command not found\n", 20);
            for (int i = 0; i < num_pipes; i++) {
                close(pipefds[i][0]);
                close(pipefds[i][1]);
            }
            for (int i = 0; i < launched; i++) waitpid(pids[i], NULL, 0);
            return 1;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            for (int i = 0; i < num_pipes; i++) {
                close(pipefds[i][0]);
                close(pipefds[i][1]);
            }
            for (int i = 0; i < launched; i++) waitpid(pids[i], NULL, 0);
            return 1;
        }

        if (pid == 0) {
            if (interactive) signal(SIGINT, SIG_DFL);

            if (s > 0)         dup2(pipefds[s-1][0], STDIN_FILENO);
            if (s < num_pipes) dup2(pipefds[s][1],   STDOUT_FILENO);

            for (int i = 0; i < num_pipes; i++) {
                close(pipefds[i][0]);
                close(pipefds[i][1]);
            }

            execv(path, clean);
            perror(path);
            exit(127);
        }

        pids[launched++] = pid;
    }

    for (int i = 0; i < num_pipes; i++) {
        close(pipefds[i][0]);
        close(pipefds[i][1]);
    }

    int last_status = 0;
    for (int s = 0; s < launched; s++) {
        int status;
        waitpid(pids[s], &status, 0);
        if (s == launched - 1) {
            if (WIFEXITED(status))        last_status = WEXITSTATUS(status);
            else if (WIFSIGNALED(status)) last_status = 1;
        }
    }
    return last_status;
}

int main(int argc, char *argv[]) {
    int interactive = 0;
    int last_was_signal = 0;
    int in_fd = STDIN_FILENO;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [script]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 2) {
        in_fd = open(argv[1], O_RDONLY);
        if (in_fd < 0) {
            perror(argv[1]);
            return EXIT_FAILURE;
        }
    } else {
        interactive = isatty(STDIN_FILENO);
    }

    if (interactive) {
        signal(SIGINT, SIG_IGN);
        write(STDOUT_FILENO, "Welcome to mysh!\n", 17);
    }

    char buf[BUF_SIZE];
    char *tokens[BUF_SIZE / 2];
    int last_status = 0;

    while (1) {
        if (interactive) {
            if (last_status != 0 && !last_was_signal) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Exited with status %d\n", last_status);
                write(STDOUT_FILENO, msg, strlen(msg));
            }
            print_prompt();
        }

        int len = read_line(in_fd, buf, sizeof(buf));
        if (len <= 0) break;

        int ntok = tokenize(buf, tokens, BUF_SIZE / 2);
        if (ntok == 0) continue;

        if (strcmp(tokens[0], "exit") == 0) {
            break;
        }
        if (strcmp(tokens[0], "cd") == 0) {
            last_was_signal = 0;

            if (ntok > 2) {
                write(STDERR_FILENO, "cd: too many arguments\n", 23);
                last_status = 1;
                continue;
            }

            const char *dir = (ntok == 2) ? tokens[1] : getenv("HOME");
            if (!dir) dir = "/";

            if (chdir(dir) < 0) {
                perror("cd");
                last_status = 1;
            } else {
                last_status = 0;
            }

            continue;
        }
        if (strcmp(tokens[0], "pwd") == 0) {
            last_was_signal = 0;

            char cwd[BUF_SIZE];
            if (getcwd(cwd, sizeof(cwd))) {
                write(STDOUT_FILENO, cwd, strlen(cwd));
                write(STDOUT_FILENO, "\n", 1);
                last_status = 0;
            } else {
                perror("pwd");
                last_status = 1;
            }
            continue;
        }
        if (strcmp(tokens[0], "which") == 0) {
            last_was_signal = 0;

            if (ntok != 2) {
                write(STDERR_FILENO, "which: wrong number of arguments\n", 33);
                last_status = 1;
            } else if (
                strcmp(tokens[1], "cd") == 0 ||
                strcmp(tokens[1], "pwd") == 0 ||
                strcmp(tokens[1], "which") == 0 ||
                strcmp(tokens[1], "exit") == 0
            ) {
                last_status = 1;
            } else {
                char path[BUF_SIZE];
                if (find_program(tokens[1], path, sizeof(path))) {
                    write(STDOUT_FILENO, path, strlen(path));
                    write(STDOUT_FILENO, "\n", 1);
                    last_status = 0;
                } else {
                    last_status = 1;
                }
            }
            continue;
        }

        if (has_pipe(tokens, ntok)) {
            last_status = run_pipeline(tokens, ntok, interactive);
            last_was_signal = 0;
            continue;
        }

        char path[BUF_SIZE];
        if (strchr(tokens[0], '/')) {
            snprintf(path, sizeof(path), "%s", tokens[0]);
        } else if (!find_program(tokens[0], path, sizeof(path))) {
            write(STDERR_FILENO, tokens[0], strlen(tokens[0]));
            write(STDERR_FILENO, ": command not found\n", 20);
            last_status = 1;
            last_was_signal = 0;
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); continue; }

        if (pid == 0) {
            if (interactive) {
                signal(SIGINT, SIG_DFL);
            } else {
                int devnull = open("/dev/null", O_RDONLY);
                if (devnull >= 0) { dup2(devnull, STDIN_FILENO); close(devnull); }
            }
            execv(path, tokens);
            perror(path);
            exit(127);
        }

        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            last_status = WEXITSTATUS(status);
            last_was_signal = 0;
        } else if (WIFSIGNALED(status)) {
            last_status = 1;
            last_was_signal = 1;
            if (interactive) {
                char msg[128];
                snprintf(msg, sizeof(msg), "Terminated by signal %s\n",
                         strsignal(WTERMSIG(status)));
                write(STDOUT_FILENO, msg, strlen(msg));
            }
        }
    }

    if (interactive) {
        write(STDOUT_FILENO, "Exiting mysh.\n", 14);
    }
    return EXIT_SUCCESS;
}


