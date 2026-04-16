#ifndef MYSH_H
#define MYSH_H

#include <stddef.h>

#define BUF_SIZE 4096
#define MAX_ARGS 512
#define MAX_CMDS 16

typedef struct {
    char *argv[MAX_ARGS];
    char *input_file;
    char *output_file;
} Command;

typedef struct {
    Command cmds[MAX_CMDS];
    int num_cmds;
} Pipeline;

int tokenize(char *line, char **tokens, int max_tokens);
void parse(char **tokens, int ntok, Pipeline *pipeline);
int execute(Pipeline *pipeline, int interactive);

int find_program(const char *name, char *out, size_t outlen);
int run_builtin(Command *cmd, int *last_status);
int is_builtin(const char *name);

#endif
