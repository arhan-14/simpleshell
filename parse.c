#include <string.h>
#include <stdlib.h>
#include "mysh.h"

int tokenize(char *line, char **tokens, int max_tokens) {
    int count = 0; 
    char *p = line;

    while (*p && count < max_tokens - 1) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (*p == '\0' || *p == '#') break;

        if (*p == '<' || *p == '>' || *p == '|') {
            tokens[count++] = strndup(p, 1); 
            p++; 
            continue;
        }

        char *start = p;
        while (*p && !strchr(" \t\n<>|#", *p)) p++;
        tokens[count++] = strndup(start, p - start);
    }

    tokens[count] = NULL; 
    return count;
}

void parse(char **tokens, int ntok, Pipeline *pipeline) {
    pipeline->num_cmds = 0; 
    int argc = 0;

    Command *cmd = &pipeline->cmds[pipeline->num_cmds++];
    cmd->input_file = NULL; 
    cmd->output_file = NULL;

    for (int i = 0; i < ntok; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            cmd->argv[argc] = NULL;
            cmd = &pipeline->cmds[pipeline->num_cmds++];
            cmd->input_file = NULL; 
            cmd->output_file = NULL; 
            argc = 0;
        } else if (strcmp(tokens[i], "<") == 0) {
            if (i + 1 < ntok) cmd->input_file = tokens[++i];
        } else if (strcmp(tokens[i], ">") == 0) {
            if (i + 1 < ntok) cmd->output_file = tokens[++i];
        } else { 
            if (argc < MAX_ARGS - 1) cmd->argv[argc++] = tokens[i]; 
        }
    }

    cmd->argv[argc] = NULL;
}
