#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <dirent.h>
#include <ctype.h>

#define BUF_SIZE 4096
#define INIT_CAP 16

typedef struct {
    char **tokens;
    int count;
    int cap;
} token_list_t;

static int interactive = 0;
static char *home_dir = NULL;

void token_list_add(token_list_t *tl, const char *tok) {
    if (tl->count == tl->cap) {
        tl->cap *= 2;
        tl->tokens = realloc(tl->tokens, tl->cap * sizeof(char *));
    }
    tl->tokens[tl->count++] = strdup(tok);
}

void tokenize(const char *line, token_list_t *tl) {
    int i = 0;
    while (line[i] != '\0' && line[i] != '\n') {
        if (isspace((unsigned char)line[i])) { i++; continue; }

        if (line[i] == '#') break;

        if (line[i] == '<' || line[i] == '>' || line[i] == '|') {
            char s[2] = { line[i], '\0' };
            token_list_add(tl, s);
            i++;
            continue;
        }

        int start = i;
        while (line[i] != '\0' && line[i] != '\n' &&
               !isspace((unsigned char)line[i]) &&
               line[i] != '<' && line[i] != '>' &&
               line[i] != '|' && line[i] != '#') {
            i++;
        }
        int len = i - start;
        char tok[len + 1];
        memcpy(tok, line + start, len);
        tok[len] = '\0';
        token_list_add(tl, tok);
    }
}

int main(int argc, char *argv[]) {
    return EXIT_SUCCESS;
}
