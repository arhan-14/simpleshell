#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <glob.h>
#include <signal.h>
#include "mysh.h"

int find_program(const char *name, char *out, size_t outlen) {
    const char *dirs[] = { "/usr/local/bin", "/usr/bin", "/bin", NULL };

    for (int i = 0; dirs[i]; i++) {
        snprintf(out, outlen, "%s/%s", dirs[i], name);
        if (access(out, X_OK) == 0) return 1;
    }
    return 0;
}

static void expand_wildcards(Command *cmd) {
    char *new_argv[MAX_ARGS]; 
    int new_argc = 0;

    for (int i = 0; cmd->argv[i]; i++) {
        if (strchr(cmd->argv[i], '*')) {
            glob_t g; 
            int flags = (cmd->argv[i][0] == '*') ? 0 : GLOB_NOCHECK;

            if (glob(cmd->argv[i], flags, NULL, &g) == 0) {
                for (size_t j = 0; j < g.gl_pathc && new_argc < MAX_ARGS - 1; j++) {
                    new_argv[new_argc++] = strdup(g.gl_pathv[j]);
                }
                globfree(&g);
            } else { 
                new_argv[new_argc++] = strdup(cmd->argv[i]); 
            }
        } else { 
            new_argv[new_argc++] = strdup(cmd->argv[i]); 
        }
    }

    new_argv[new_argc] = NULL;
    for (int i = 0; i < new_argc; i++) cmd->argv[i] = new_argv[i];
    cmd->argv[new_argc] = NULL;
}

int execute(Pipeline *pipeline, int interactive) {
    int n = pipeline->num_cmds;
    int num_pipes = n - 1;
    int p_fds[MAX_CMDS][2];
    int all_fds[MAX_CMDS * 2];

    for (int i = 0; i < num_pipes; i++) {
        pipe(p_fds[i]); 
        all_fds[i * 2] = p_fds[i][0]; 
        all_fds[i * 2 + 1] = p_fds[i][1];
    }

    pid_t pids[MAX_CMDS]; 
    int last_status = 0;

    for (int i = 0; i < n; i++) {
        Command *cmd = &pipeline->cmds[i]; 
        expand_wildcards(cmd);

        if (n == 1 && is_builtin(cmd->argv[0])) return run_builtin(cmd, &last_status);

        pids[i] = fork();
        if (pids[i] == 0) {
            if (interactive) signal(SIGINT, SIG_DFL);

            int in = (i == 0) ? STDIN_FILENO : p_fds[i - 1][0];
            int out = (i == n - 1) ? STDOUT_FILENO : p_fds[i][1];

            dup2(in, STDIN_FILENO); 
            dup2(out, STDOUT_FILENO);

            for (int j = 0; j < num_pipes * 2; j++) close(all_fds[j]);

            if (cmd->input_file) {
                int fd = open(cmd->input_file, O_RDONLY);
                if (fd < 0) exit(1); 
                dup2(fd, STDIN_FILENO); 
                close(fd);
            }

            if (cmd->output_file) {
                int fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0640);
                if (fd < 0) exit(1); 
                dup2(fd, STDOUT_FILENO); 
                close(fd);
            }

            if (is_builtin(cmd->argv[0])) exit(run_builtin(cmd, &last_status));

            char path[BUF_SIZE];
            if (strchr(cmd->argv[0], '/')) {
                execv(cmd->argv[0], cmd->argv);
            } else if (find_program(cmd->argv[0], path, sizeof(path))) {
                execv(path, cmd->argv);
            }
            exit(127);
        }
    }

    for (int i = 0; i < num_pipes * 2; i++) close(all_fds[i]);

    for (int i = 0; i < n; i++) {
        int s; 
        waitpid(pids[i], &s, 0);
        if (i == n - 1) {
            if (WIFEXITED(s)) last_status = WEXITSTATUS(s);
            else if (WIFSIGNALED(s)) last_status = 128 + WTERMSIG(s);
        }
    }

    return last_status;
}
