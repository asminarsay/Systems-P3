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

static char leftover[BUF_SIZE];
static int  leftover_len = 0;


int read_line(int fd, char *out, int out_size);
void tokenize(const char *line, token_list_t *tl);
void expand_wildcard(const char *token, token_list_t *tl);

int main(int argc, char *argv[]);
