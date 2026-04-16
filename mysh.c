#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <dirent.h>
#include <ctype.h>
#include <signal.h>

#define BUF_SIZE 4096
#define INIT_CAP 16

typedef struct{
    char **tokens;
    int count;
    int cap;
} token_list_t;

static int interactive = 0;
static char *home_dir = NULL;

void token_list_add(token_list_t *tl, char *tok){
    if(tl->count + 1 >= tl->cap){
        tl->cap *= 2;
        tl->tokens = realloc(tl->tokens, tl->cap * sizeof(char *));
    }
    tl->tokens[tl->count++] = strdup(tok);
    tl->tokens[tl->count] = NULL;
}

void tokenize(char *line, token_list_t *tl){
    int i = 0;
    while(line[i] != '\0' && line[i] != '\n'){
        if (isspace((unsigned char)line[i])){ 
            i++; 
            continue;
        }

        if (line[i] == '#'){
            break;
        }

        if(line[i] == '<' || line[i] == '>' || line[i] == '|'){
            char s[2] = { line[i], '\0' };
            token_list_add(tl, s);
            i++;
            continue;
        }

        int start = i;
        while(line[i] != '\0' && line[i] != '\n' && !isspace((unsigned char)line[i]) && line[i] != '<' && line[i] != '>' && line[i] != '|' && line[i] != '#'){
            i++;
        }
        int len = i - start;
        char tok[len + 1];
        memcpy(tok, line + start, len);
        tok[len] = '\0';
        token_list_add(tl, tok);
    }
}

void expand_wildcard(char *token, token_list_t *tl){
    if (!strchr(token, '*')){
        token_list_add(tl, token);
        return;
    }

    char *last_slash = strrchr(token, '/');
    char *pattern;
    char dir[BUF_SIZE];

    if (last_slash){
        int dir_len = last_slash - token;
        memcpy(dir, token, dir_len);
        dir[dir_len] = '\0';
        pattern = last_slash + 1;
    } else{
        strcpy(dir, ".");
        pattern = token;
    }

    char *star = strchr(pattern, '*');
    int pre_len = star - pattern;
    char *suffix = star + 1;
    int suf_len = strlen(suffix);

    DIR *dp = opendir(dir);
    if(!dp){
        token_list_add(tl, token);
        return;
    }

    char *matches[BUF_SIZE];
    int nmatches = 0;

    struct dirent *ent;
    while ((ent = readdir(dp)) != NULL){
        char *name = ent->d_name;

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

    if (nmatches == 0){
        token_list_add(tl, token);
        return;
    }

    for (int i = 1; i < nmatches; i++){
        char *key = matches[i];
        int j = i - 1;
        while (j >= 0 && strcmp(matches[j], key) > 0){
            matches[j + 1] = matches[j];
            j--;
        }
        matches[j + 1] = key;
    }

    for (int i = 0; i < nmatches; i++){
        token_list_add(tl, matches[i]);
        free(matches[i]);
    }
}

int parse_redirection(token_list_t *tl, char **infile, char **outfile){
    *infile = NULL;
    *outfile = NULL;

    token_list_t clean;
    clean.tokens = malloc(tl->cap * sizeof(char *));
    clean.count = 0;
    clean.cap = tl->cap;

    int ok = 1;
    int i = 0;
    while(i < tl->count){
        char *tok = tl->tokens[i];
        if(strcmp(tok,"<") == 0 || strcmp(tok,">") == 0){
            if(i + 1 >= tl->count){
                fprintf(stderr,"syntax error near %s\n",tok);
                ok = 0;
                i++;
                continue;
            }
            char *next = tl->tokens[i+1];
            if(strcmp(next,"<") == 0 || strcmp(next,">") == 0 || strcmp(next,"|") == 0){
                fprintf(stderr,"syntax error near %s\n",tok);
                ok = 0;
                i += 2;
                continue;
            }
            if(tok[0] == '<'){
                free(*infile);
                *infile = strdup(next);
            }
            else{
                free(*outfile);
                *outfile = strdup(next);
            }
            i += 2;
        } else{
            clean.tokens[clean.count++] = strdup(tok);
            i++;
        }
    }

    clean.tokens[clean.count] = NULL;

    for(int j=0; j<tl->count; j++){
        free(tl->tokens[j]);
    }
    free(tl->tokens);
    tl->tokens = clean.tokens;
    tl->count = clean.count;
    tl->cap = clean.cap;

    if(ok){
        return 0;
    }
    return -1;
}

int apply_redirection(char *infile, char *outfile, int in_pipe){
    if(infile){
        int fd = open(infile, O_RDONLY);
        if(fd < 0){
            printf("error opening %s\n", infile);
            return -1;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    } else if(!interactive && !in_pipe){
        int fd = open("/dev/null", O_RDONLY);
        if(fd >= 0){
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
    }
    if(outfile){
        int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
        if(fd < 0){
            printf("error opening %s\n", outfile);
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    return 0;
}

void token_list_init(token_list_t *tl){
    tl->tokens = malloc(INIT_CAP * sizeof(char *));
    tl->count = 0;
    tl->cap = INIT_CAP;
}

void token_list_free(token_list_t *tl){
    for(int i=0; i<tl->count; i++){
        free(tl->tokens[i]);
    }
    free(tl->tokens);
    tl->tokens = NULL;
    tl->count = 0;
    tl->cap = 0;
}


// search through the three paths given and return the path to the program
// usr/local/bin, /usr/bin, and /bin, in that order
char* bare_name_search(char *program){
    char* dir_to_check[] = {"/usr/local/bin", "/usr/bin", "/bin"};
    
    for(int i=0; i<3; i++){
        
        char buffer[1024];
        snprintf(buffer,sizeof(buffer),"%s/%s",dir_to_check[i],program);
        if(access(buffer,X_OK) == 0){
            // readable and executable
            return strdup(buffer);
        }
    }

    return NULL;
}


// executes program for bare name
int bare_names(token_list_t *tl, int isPipe){

    if(strchr(tl->tokens[0],'/') != NULL){
        pid_t pid = fork();
        if(pid == 0){
            if(!interactive && !isPipe){
                int fd = open("/dev/null", O_RDONLY);
                if(fd >= 0){
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
            }
            if(access(tl->tokens[0],X_OK) == 0){
                execv(tl->tokens[0],tl->tokens);
                exit(1);
            }
            else{
                if(isPipe == 0){
                    printf("error: this path does not exist\n");
                }
                exit(1);
            }
        }
        else if(pid > 0){
            int status;
            waitpid(pid,&status,0);
            return status;
        }
        else{
            if(isPipe == 0){
                printf("error: in fork()\n");
            }
            return -1;
        }
    }

    // uses the search of names and then stores it so it can be used in access
    char *path = bare_name_search(tl->tokens[0]);
    if(path == NULL){
        if(isPipe == 0){
            printf("error: bare_names: program not found\n");
        }
        return -1;
    }

    pid_t pid = fork();

    if(pid == 0){ // in child process
        if(!interactive && !isPipe){
            int fd = open("/dev/null", O_RDONLY);
            if(fd >= 0){
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
        }
        execv(path, tl->tokens);
        exit(1);
    }
    else if(pid > 0){ // in parent process
        int status;
        waitpid(pid,&status,0);
        free(path);
        return status;
    }
    else{
        if(isPipe == 0){
            printf("error: in fork()\n");
        }
        free(path);
        return -1;
    }
}


int exec_redirect(token_list_t *tl, char *infile, char *outfile){
    char *path;
    if(strchr(tl->tokens[0],'/') != NULL){
        path = strdup(tl->tokens[0]);
    } else{
        path = bare_name_search(tl->tokens[0]);
    }
    if(path == NULL){
        printf("%s: command not found\n",tl->tokens[0]);
        return -1;
    }

    pid_t pid = fork();

    if(pid == 0){
        if(apply_redirection(infile, outfile, 0) < 0){
            exit(1);
        }
        execv(path, tl->tokens);
        printf("exec failed\n");
        exit(1);
    }
    else if(pid > 0){
        int status;
        waitpid(pid,&status,0);
        free(path);
        return status;
    }
    else{
        printf("error in fork()\n");
        free(path);
        return -1;
    }
}

// checks for built in commands within which
int which_check(token_list_t *tl){
    for(int i=1; i<tl->count; i++){
        if(strcmp(tl->tokens[1],"cd") == 0 || strcmp(tl->tokens[1],"pwd") == 0 || strcmp(tl->tokens[1],"which") == 0 || strcmp(tl->tokens[1],"exit") == 0){
            return -1;
        }
    }
    return 0;
}

// built in functions include:
    // cd, pwd, which, and exit
int built_in(token_list_t *tl, int isPipe){

    if(strcmp(tl->tokens[0],"cd") == 0){
        // checks for excess of parameters
        char *newDir;
        if(tl->count == 1){
            newDir = getenv("HOME");
        }
        else if (tl->count == 2){
            newDir = tl->tokens[1];
        }
        else{
            printf("cd: too many arguments\n");
            return 1;
        }

        int success = chdir(newDir);
        if(success != 0){
            printf("cd: no such file or directory\n");
            return 1;
        }
        return 0;
    }

    else if(strcmp(tl->tokens[0],"pwd") == 0){

        char buffer[1024];
        char *currDir = getcwd(buffer,sizeof(buffer));
        if(currDir == NULL){
            if(isPipe == 0){
                printf("pwd: no working directory found\n");
            }
            return 1;
        }
        else{
            printf("%s\n",buffer);
        }
        return 0;
    }

    else if(strcmp(tl->tokens[0],"which") == 0){

        if(tl->count != 2){
            if(isPipe == 0){
                printf("error: which: too many arguments\n");
            }
            return 1;
        }
        if(which_check(tl) == -1){
            if(isPipe == 0){
                printf("error: which: cannot use built in in which\n");
            }
            return 1;
        }
        else{
            char *path = bare_name_search(tl->tokens[1]);
            if(path == NULL){
                if(isPipe == 0){
                    printf("error: which: bare_name path not found\n");
                }
                return 1;
            }
            printf("%s\n",path);
            free(path);
            return 0;
        }

    }

    else if(strcmp(tl->tokens[0],"exit") == 0){
        exit(0);
    }

    return 1;
}


int builtin_redirect(token_list_t *tl, char *infile, char *outfile){
    pid_t pid = fork();
    if(pid == 0){
        if(apply_redirection(infile, outfile, 0) < 0){
            exit(1);
        }
        built_in(tl,0);
        exit(0);
    }
    else if(pid > 0){
        int status;
        waitpid(pid, &status, 0);
        return status;
    }
    else{
        printf("error in fork()\n");
        return -1;
    }
}


int apply_piping(token_list_t *tl, int *has_exit){
    *has_exit = 0;

    // splits all piping into their own tasks
    int num_pipes = 0;
    for(int i=0; i<tl->count; i++){
        if(strcmp(tl->tokens[i],"|") == 0){
            num_pipes++;
        }
    }

    token_list_t split[num_pipes+1];
    for(int i=0; i<num_pipes+1; i++){
        token_list_init(&split[i]);
    }

    int counter = 0;
    token_list_t *current = &split[counter];

    for(int i=0; i<tl->count; i++){
        if(strcmp(tl->tokens[i],"|") == 0){
            counter++;
            current = &split[counter];
        }
        else{
            token_list_add(current,tl->tokens[i]);
        }
    }

    for(int i=0; i<num_pipes+1; i++){
        if(split[i].count > 0 && strcmp(split[i].tokens[0],"exit") == 0){
            *has_exit = 1;
        }
    }

    int pre_defined_pipes[num_pipes][2];

    for(int i=0; i<num_pipes; i++){
        int temp = pipe(pre_defined_pipes[i]);

        if(temp == -1){
            printf("error creating pipes\n");
            for(int j=0; j<num_pipes+1; j++){
                token_list_free(&split[j]);
            }
            return -1;
        }
    }

    pid_t pids[num_pipes+1];

    for(int i=0; i<num_pipes+1; i++){
        pids[i] = fork();

        if(pids[i] == 0){ // in child process

            if(i == 0 && !interactive){
                int fd = open("/dev/null", O_RDONLY);
                if(fd >= 0){
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
            }

            if(i != 0){ // this is not the first index in the list
                dup2(pre_defined_pipes[i-1][0],STDIN_FILENO);
            }
            if(i != num_pipes){ // this is not the last index in the list
                dup2(pre_defined_pipes[i][1],STDOUT_FILENO);
            }

            for(int j=0; j<num_pipes; j++){
                close(pre_defined_pipes[j][0]);
                close(pre_defined_pipes[j][1]);
            }

            // check for built in or bare
            if(split[i].count == 0){
                exit(1);
            }
            if(strcmp(split[i].tokens[0],"cd") == 0 || strcmp(split[i].tokens[0],"pwd") == 0 || strcmp(split[i].tokens[0],"which") == 0 || strcmp(split[i].tokens[0],"exit") == 0){
                built_in(&split[i],1);
                exit(0);
            }
            else{
                bare_names(&split[i],1);
                exit(0);
            }
        }
    }

    for(int i=0; i<num_pipes; i++){
        close(pre_defined_pipes[i][0]);
        close(pre_defined_pipes[i][1]);
    }

    int last_status = 0;
    for(int i=0; i<num_pipes+1; i++){
        int status;
        waitpid(pids[i], &status, 0);
        if(i == num_pipes){
            last_status = status;
        }
    }

    for(int i=0; i<num_pipes+1; i++){
        token_list_free(&split[i]);
    }

    return last_status;
}



static char leftover[BUF_SIZE];
static int leftover_len = 0;

int read_line(int fd, char *out, int out_size){
    int pos = 0;

    // use up any leftover bytes from last read
    while(leftover_len > 0 && pos < out_size - 1){
        char c = leftover[0];
        memmove(leftover, leftover+1, --leftover_len);
        out[pos++] = c;
        if(c == '\n'){
            out[pos] = '\0';
            return pos;
        }
    }

    char tmp[BUF_SIZE];
    while(pos < out_size - 1){
        int n = read(fd, tmp, sizeof(tmp));
        if(n <= 0){
            if(pos == 0) return -1;
            out[pos] = '\0';
            return pos;
        }
        for(int i=0; i<n; i++){
            if(tmp[i] == '\n'){
                out[pos++] = '\n';
                out[pos] = '\0';
                // save the rest for next call
                int rem = n - i - 1;
                if(rem > 0){
                    memcpy(leftover, tmp+i+1, rem);
                    leftover_len = rem;
                }
                return pos;
            }
            out[pos++] = tmp[i];
            if(pos >= out_size - 1) break;
        }
    }
    out[pos] = '\0';
    return pos;
}

int main(int argc, char *argv[]) {
    int input_fd;

    if (argc > 2) {
        fprintf(stderr, "too many arguments\n");
        return EXIT_FAILURE;
    }

    if (argc == 2) { // batch mode
        input_fd = open(argv[1], O_RDONLY);
        if (input_fd < 0) { 
            perror(argv[1]); 
            return EXIT_FAILURE; 
        }
        interactive = 0;
    } 
    else { // interactive mode
        input_fd = STDIN_FILENO;
        interactive = isatty(STDIN_FILENO);
    }

    home_dir = getenv("HOME");
    if (!home_dir) home_dir = "/";

    if (interactive) write(STDOUT_FILENO, "Welcome to my shell!\n", 21);

    int should_exit = 0;
    char line[BUF_SIZE];

    while (!should_exit) {
        fflush(stdout);

        if(interactive){
            char currwd[1024];
            getcwd(currwd,sizeof(currwd));
            int hlen = strlen(home_dir);
            if(hlen > 1 && strncmp(currwd, home_dir, hlen) == 0
               && (currwd[hlen] == '/' || currwd[hlen] == '\0')){
                write(STDOUT_FILENO, "~", 1);
                if(currwd[hlen] != '\0')
                    write(STDOUT_FILENO, currwd + hlen, strlen(currwd + hlen));
                write(STDOUT_FILENO, "$ ", 2);
            }
            else{
                write(STDOUT_FILENO, currwd, strlen(currwd));
                write(STDOUT_FILENO, "$ ", 2);
            }
        }

        if (read_line(input_fd, line, sizeof(line)) == -1) {
            break;
        }

        // tokenize the line
        token_list_t tl;
        token_list_init(&tl);
        tokenize(line, &tl);

        if(tl.count == 0){
            token_list_free(&tl);
            continue;
        }

        // expand wildcards but skip special tokens
        token_list_t expanded;
        token_list_init(&expanded);
        for(int i=0; i<tl.count; i++){
            if(strcmp(tl.tokens[i],"<") == 0 || strcmp(tl.tokens[i],">") == 0 || strcmp(tl.tokens[i],"|") == 0){
                token_list_add(&expanded, tl.tokens[i]);
            }
            else{
                expand_wildcard(tl.tokens[i], &expanded);
            }
        }
        
        token_list_free(&tl);

        // pull out redirection
        char *infile, *outfile;
        int redir_ok = parse_redirection(&expanded, &infile, &outfile);

        if(redir_ok < 0){
            free(infile);
            free(outfile);
            token_list_free(&expanded);
            continue;
        }

        if(expanded.count == 0){
            free(infile);
            free(outfile);
            token_list_free(&expanded);
            continue;
        }

        int found_pipe = 0;
        int wstatus = 0;
        int from_child = 0; // 1 if wstatus came from waitpid

        for(int i=0; i<expanded.count; i++){
            if(strcmp(expanded.tokens[i],"|") == 0){
                found_pipe = 1;
                break;
            }
        }

        if(found_pipe){
            int has_exit = 0;
            wstatus = apply_piping(&expanded, &has_exit);
            from_child = 1;
            if(has_exit) should_exit = 1;
            free(infile);
            free(outfile);
            token_list_free(&expanded);
        }
        else if(strcmp(expanded.tokens[0],"exit") == 0){
            should_exit = 1;
            free(infile);
            free(outfile);
            token_list_free(&expanded);
        }
        else if(strcmp(expanded.tokens[0],"cd") == 0 || strcmp(expanded.tokens[0],"pwd") == 0 || strcmp(expanded.tokens[0],"which") == 0){
            if(infile || outfile){
                wstatus = builtin_redirect(&expanded, infile, outfile);
                from_child = 1;
            }
            else{
                wstatus = built_in(&expanded,0);
                from_child = 0;
            }
            free(infile);
            free(outfile);
            token_list_free(&expanded);
        }
        else if(infile || outfile){
            wstatus = exec_redirect(&expanded, infile, outfile);
            from_child = 1;
            free(infile);
            free(outfile);
            token_list_free(&expanded);
        }
        else{
            wstatus = bare_names(&expanded,0);
            from_child = 1;
            free(infile);
            free(outfile);
            token_list_free(&expanded);
        }

        // show exit status if needed
        if(interactive && !should_exit && from_child && wstatus >= 0){
            if(WIFSIGNALED(wstatus)){
                char buf[256];
                int len = snprintf(buf,sizeof(buf),"Terminated by signal %d: %s\n",WTERMSIG(wstatus),strsignal(WTERMSIG(wstatus)));
                write(STDOUT_FILENO,buf,len);
            }
            else if(WIFEXITED(wstatus) && WEXITSTATUS(wstatus) != 0){
                char buf[64];
                int len = snprintf(buf,sizeof(buf),"Exited with status %d\n",WEXITSTATUS(wstatus));
                write(STDOUT_FILENO,buf,len);
            }
        }
    }

    if (interactive) write(STDOUT_FILENO, "Exiting my shell.\n", 18);
    if (input_fd != STDIN_FILENO) close(input_fd);
    return EXIT_SUCCESS;
}
