Authors: ndg53 adn54

Tokenization

Tokenization works by breaking a string down to individual tokens or parts using whitespace for separation. The characters <, >, and | will always create their own token. The character #, will cause everything else in the line to be ignored. Leading and trailing whitespace from blank lines will be removed.

Wildcard Expansion

When wildcard expansion occurs, all tokens containing an asterisk (*) are passed to the expand_wildcard function, which extracts three pieces of information from the token(s) being processed. These include: (1) directory path to search; (2), file name prefix to match; and (3) file name suffix to match. The readdir() and opendir() system calls will be used to do this iteration. For tokens starting with a period (.) will not be considered unless the wildcard expression also begins with a period (.)). The matched tokens will be sorted and replaced in place; if no match is found, the token will remain as is.

Redirection

The parse_redirection function processes the list of tokens to locate instances of < and >. When either is found, the subsequent filename will be saved, and both tokens will be removed. The parse_redirection function will also detect cases where there are redirects in succession and if the last token in the token list is a redirect. The actual connecting of files to stdin and stdout is performed in apply_redirection by using dup2 called from the child process after forking. Output files will be created with a file mode of 0640 and, if no file path is passed to the input parameter during the call to apply_redirection, stdin will be connected to the special file /dev/null when operating in batch mode.

Redirected Executions

The functions exec_redirect and builtin_redirect are simply wrappers, but their only purpose is handling the redirection of output. The way exec_redirect does this is through an initial call to fork(), which creates a child process to run apply_redirection(), after which the child process is run using execv(). All we've really done is add another layer of abstraction on to this system. Builtin_redirect serves as a wrapper for execution of built-in commands as well as providing the same level of abstraction for built-ins without actually redirecting output from the shell to the stdout stream via fork().

NOTE: When redirection occurs, all errors for either path or command will be redirected to the output file.

Built-In Commands

Using the function strcmp(), we can determine whether the first token provided to the shell is a built-in command and therefore can be run without forking a child process. cd, pwd, and exit will print error messages if something goes wrong (like wrong number of arguments or a bad path). which is the exception, the spec says it should print nothing and just fail silently if it gets the wrong number of arguments, a built-in name, or a program that doesnt exist. The shell keeps going after any error so the user can try again.

Bare Name Commands

bare_names looks for the possibility of executing the token directly first. If the token is not directly executable, bare_names_search is invoked to check the directories /usr/local/bin, /usr/bin and /bin. If the directory is valid (we can fork and execute the command) or NULL (and halt), the pipeline will not exit (it will be terminated only if this is the last command), which means that a command that fails during the execution does not produce any output, so the next command in the pipeline sees an empty input but continues execution of the pipeline. For example: ls | does_not_exist | wc does not produce any output; the expected result is zero (0) for each output field (standard output, standard error and exit status).

Pipelines

After breaking the string into separate tokens separated by "|", we return an array of tokens to the command. The pipe() calls are performed prior to any calls to fork(). Each fork result gets stored in a pids array so we can waitpid on specific children later. All fd pairs are placed into a 2D array, so that all children that get forked will know the pipe they should read or write to when they get created. If a command fails during the pipeline, it does not write any output, so the next command in the pipeline sees there is no input and continues to execute its code. The exit status of the pipeline comes from the last process.

Main

argc gets checked first. One argument opens that file and sets batch mode. No arguments reads from stdin, and isatty() sorts out whether thats interactive or batch. More than one argument just prints usage and exits.

HOME comes from getenv("HOME") early on so the prompt can use it. If interactive we print a welcome message before the loop starts.

read_line is how we get input. It uses read() to pull a chunk of bytes from the input fd into a temp buffer, then scans through that chunk for a newline. Once it finds one, everything up to and including it gets returned as the current line, and whatever came after the newline gets saved in a static leftover buffer for the next call. That way we always get exactly one complete command per call and dont block waiting for more input when theres already a full command sitting there.

Inside the loop we flush stdout first so any buffered printf output from the previous command actually shows up in the right order. Then if were interactive we print the prompt, which is the current working directory with the home directory prefix swapped out for ~ followed by a dollar sign and a space. We call read_line to grab one line and if it comes back -1 thats EOF so we break out.

From there we tokenize the line, skip it if its empty, then expand any wildcards. After that parse_redirection pulls out < and > tokens along with their filenames. If theres a syntax error it prints a message and we skip the command. If the token list is empty after redirection parsing we also skip it so we dont crash on a NULL access.

Then we check if theres a pipe in the tokens. If so we hand everything off to apply_piping which also checks if any segment of the pipeline is an exit command and reports that back. If not a pipe, we check if the first token is exit and if so we set should_exit and let the loop end naturally so the goodbye message at the bottom of main actually runs. If the first token is a built-in (cd, pwd, which) and theres redirection we use builtin_redirect which forks so the redirect doesnt mess with the parent, otherwise we just call built_in directly. If its not a built-in and theres redirection we go through exec_redirect, and if theres no redirection at all we just call bare_names which handles both path-based executables (if it has a /) and searching through /usr/local/bin, /usr/bin, and /bin.

After each command, if were in interactive mode we check the wait status. If the command exited with a non-zero code we print "Exited with status N" and if it got killed by a signal we print "Terminated by signal".

Once the loop ends, either from the exit command or EOF, we print a goodbye message if interactive and close the file descriptor if we opened one. Main always returns EXIT_SUCCESS unless it couldnt open the script file.

Testing

We made a test/ directory with multiple test scripts. edge_cases.sh covers the basics like normal built-in and bare name commands, but also the weird stuff -- built-ins with wrong argument counts, which on a built-in (which should fail silently), redirection into files, and multi-stage pipelines. We also specifically tested what happens when a middle pipe stage fails, plus wildcard expansion, trying to cd somewhere that doesnt exist, and a normal exit at the end.

We also have separate test scripts for specific features:
    - test_builtins.sh: cd to different directories, pwd, which with valid/invalid/built-in args
    - test_redirection.sh: output redirect, input redirect, both at once, both orderings, built-in redirect
    - test_pipes.sh: single pipes, multi-stage pipes, built-ins in pipes, long chains
    - test_wildcards.sh: *.c, *.h (no match), test/* (subdir glob), no-match stays as-is
    - test_comments.sh: full line comments, inline comments, leading whitespace comments
    - test_syntax_errors.sh: < <, > >, bare redirects, | at start, double ||, recovery after errors
    - test_paths.sh: /bin/echo, /bin/ls, nonexistent paths
    - test_empty.sh: blank lines, multiple blank lines
    - test_exit_pipe.sh: echo | exit terminates shell, commands after dont run
