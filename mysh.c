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

//adds new token string to dynamic array
void token_list_add(token_list_t *tl, const char *tok) {
    if (tl->count == tl->cap) {
        tl->cap *= 2;
        tl->tokens = realloc(tl->tokens, tl->cap * sizeof(char *));
    }
    tl->tokens[tl->count++] = strdup(tok);
}

//suppose to take in raw line of text to split and token list
void tokenize(const char *line, token_list_t *tl) {
    int i = 0;
    while (line[i] != '\0' && line[i] != '\n') {
        if (isspace((unsigned char)line[i])){ 
            i++; 
            continue;
        }

        if (line[i] == '#'){
            break;
        }

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

void expand_wildcard(const char *token, token_list_t *tl) {
    if (!strchr(token, '*')) {
        token_list_add(tl, token);
        return;
    }

    const char *last_slash = strrchr(token, '/');
    const char *pattern;
    char dir[BUF_SIZE];

    if (last_slash) {
        int dir_len = last_slash - token;
        memcpy(dir, token, dir_len);
        dir[dir_len] = '\0';
        pattern = last_slash + 1;
    } else {
        strcpy(dir, ".");
        pattern = token;
    }

    const char *star = strchr(pattern, '*');
    int pre_len = star - pattern;
    const char *suffix = star + 1;
    int suf_len = strlen(suffix);

    DIR *dp = opendir(dir);
    if (!dp) {
        token_list_add(tl, token);
        return;
    }

    char *matches[BUF_SIZE];
    int nmatches = 0;

    struct dirent *ent;
    while ((ent = readdir(dp)) != NULL) {
        const char *name = ent->d_name;

        if (name[0] == '.' && pattern[0] != '.') continue;

        int nlen = strlen(name);
        if (nlen < pre_len + suf_len) continue;
        if (strncmp(name, pattern, pre_len) != 0) continue;
        if (suf_len > 0 && strcmp(name + nlen - suf_len, suffix) != 0) continue;

        char full[BUF_SIZE];
        if (strcmp(dir, ".") == 0)
            snprintf(full, sizeof(full), "%s", name);
        else
            snprintf(full, sizeof(full), "%s/%s", dir, name);

        matches[nmatches++] = strdup(full);
    }
    closedir(dp);

    if (nmatches == 0) {
        token_list_add(tl, token);
        return;
    }

    for (int i = 1; i < nmatches; i++) {
        char *key = matches[i];
        int j = i - 1;
        while (j >= 0 && strcmp(matches[j], key) > 0) {
            matches[j + 1] = matches[j];
            j--;
        }
        matches[j + 1] = key;
    }

    for (int i = 0; i < nmatches; i++) {
        token_list_add(tl, matches[i]);
        free(matches[i]);
    }
}

// returns 0 on success, -1 on syntax error
// pulls < and > out of tl, sets infile/outfile, rebuilds tl without them
int parse_redirection(token_list_t *tl, char **infile, char **outfile) {
    *infile = NULL;
    *outfile = NULL;

    token_list_t clean;
    clean.tokens = malloc(tl->cap * sizeof(char *));
    clean.count = 0;
    clean.cap = tl->cap;

    int ok = 1;
    int i = 0;
    while (i < tl->count) {
        char *tok = tl->tokens[i];
        if (strcmp(tok, "<") == 0 || strcmp(tok, ">") == 0) {
            if (i + 1 >= tl->count) {
                fprintf(stderr, "syntax error near %s\n", tok);
                ok = 0;
                i++;
                continue;
            }
            char *next = tl->tokens[i + 1];
            if (strcmp(next, "<") == 0 || strcmp(next, ">") == 0 || strcmp(next, "|") == 0) {
                fprintf(stderr, "syntax error near %s\n", tok);
                ok = 0;
                i += 2;
                continue;
            }
            if (tok[0] == '<') { free(*infile); *infile = strdup(next); }
            else               { free(*outfile); *outfile = strdup(next); }
            i += 2;
        } else {
            clean.tokens[clean.count++] = strdup(tok);
            i++;
        }
    }

    for (int j = 0; j < tl->count; j++) free(tl->tokens[j]);
    free(tl->tokens);
    tl->tokens = clean.tokens;
    tl->count = clean.count;
    tl->cap = clean.cap;

    return ok ? 0 : -1;
}

int apply_redirection(const char *infile, const char *outfile, int in_pipe) {
    if (infile) {
        int fd = open(infile, O_RDONLY);
        if (fd < 0) {
            perror(infile);
            return -1;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    } else if (!interactive && !in_pipe) {
        int fd = open("/dev/null", O_RDONLY);
        if (fd >= 0) {
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
    }
    if (outfile) {
        int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
        if (fd < 0) {
            perror(outfile);
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    return EXIT_SUCCESS;
}
