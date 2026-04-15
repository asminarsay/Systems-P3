Authors: adn54 ndg53

Tokenization
    tokenize is what breaks the input up. It walks the line one character at a time, splitting on whitespace. <, >, and | are their own tokens regardless of spacing around them. A # throws out whatever's left on the line. Empty lines just get skipped.
    Wildcard Expansion
    expand_wildcard gets called on any token that has a * somewhere in it. We figure out what directory to look in, then what the name has to start with and end with. opendir and readdir let us go through the entries and we check each one against those two parts. Anything starting with a dot gets ignored except when the pattern started with one too. Matches get alphabetically sorted then substituted in. No matches means the token just stays how it was.

Redirection
    Two functions handle this. parse_redirection goes through the tokens and when it finds a < or > it takes the filename that comes right after and stores it, then those tokens get removed from the list. We also check for bad cases there like a redirect followed immediately by another redirect or nothing at all. apply_redirection is what runs in the child process once we fork. It opens whichever files were specified and dup2 handles wiring them to stdin or stdout. 0640 is the mode for output files. Batch mode gets stdin pointed to /dev/null if nobody specified an input file.

Execution with Redirection
    exec_redirect and builtin_redirect are wrappers we added around the existing execution code. exec_redirect forks, calls apply_redirection in the child, then execv runs the program. builtin_redirect does basically the same thing but for built-in commands. The reason we fork for built-ins at all is that otherwise there's no way to redirect their output without it going to the terminal anyway. If there's nothing to redirect we don't bother with the wrappers, bare_names and built_in get called like normal.
    NOTE: if there is an error with finding the path or any command in redirect methods, it will send the error message to the output file

Built In commands
    The Program uses the tokenized values and strcmp() to detect whether a built in command is being used
    for built in commands, we did not need to start any child processes as they are built into the shell 
    If any errors are detected, processesing the command will stop, and it will print an error message to either the command line or the file the command is being redirected to. If the user makes the mistkae of having the wrong amount or types of arguments, it will let them try again as the loop does not end if there are any errors detected.

Bare Name Commands
    The function bare_names also detects for executable files in the ery beginning. If it determines that the tokens are not executable files, it will go on to search for the program within the three given path, using bare_names_search. 
    If a path is obtained it runs it within a child process, if the path returns as NULL from the search the process will stop.
    This allows for pipelines to continue, because although an error was hit, it will stop the command and pass on NULL to the next pipe, thus allowing the pipeline to continue. 
    - For example: if which ls | does_not_exit | wc was run, it would return 0 0 0 because does_not_exist will return nothing to wc

Pipelines
    Here, we create a list of token_list_t structs so that each process, divided by the pipes, is treated independently of one another before we pass on information between the pipes.
    - for example: which ls | grep bin | wc --> a list of token lists: [["which", "ls"], ["grep", "bin"], ["wc" ]]
    The pipelines are created and stored before the actual forking of child processes begin. These pipelines are stored in a two dimensional array so that they can be stored and later be referred to when usinig the dup2() method. 
    If any issues emerge in the pipeline when it comes to forking, it will print an error and return. 
    If any issues emerge with the components of the pipes, aka the separate commands, the process will continue to the last process in the pipeline, but will feed it NULL information, such as the example shown in the previous section.

Main
    The main method checks arguments first to figure out which mode we're in. If there's one argument we open that file as the input source and set it to batch. If there's no argument we read from stdin and use isatty() to decide between interactive and batch. More than one argument just prints a usage message and exits.
    We grab the HOME directory with getenv("HOME") at startup so we can use it for the prompt later. If we're interactive we print a welcome message before entering the loop.
    read_line is how we get input. It uses read() to pull bytes from the input fd into a temp buffer and scans for a newline. Once it finds one, everything up to and including it gets returned as the current line, and whatever's left over gets saved in a static buffer for next time. That way we always get exactly one complete command per call and don't block waiting for more input when there's already a full command sitting there.
    Inside the loop, if we're interactive we print the prompt. The prompt is the current working directory followed by dollar sign and a space. If the cwd starts with the home directory path that prefix gets swapped out for ~. Then we call read_line to grab one line. If it comes back -1 that means EOF so we break out.
    From there we tokenize the line, skip it if it's empty, then expand any wildcards. After that parse_redirection pulls out < and > tokens along with their filenames. If there's a syntax error there it prints a message and we skip the command.
    Then we check if there's a pipe in the tokens. If so we hand everything off to apply_piping and move on. If not, we check if the first token is a built-in (cd, pwd, which, exit). If there's redirection we use builtin_redirect which forks so the redirect doesn't mess with the parent, otherwise we just call built_in directly. If it's not a built-in and there's redirection we go through exec_redirect, and if there's no redirection at all we just call bare_names which handles both path-based executables (if it has a /) and searching through /usr/local/bin, /usr/bin, and /bin.
    Once the loop ends, either from the exit command or EOF, we print a goodbye message if interactive and close the file descriptor if we opened one. Main always returns EXIT_SUCCESS unless it couldn't open the script file.

Testing
    We created a directory called test which contains a file called edge_cases.sh
    In this file, we tested all sorts of things such as:
        - normal bare name and built in commands
        - instances where built in commands are given too many arguments or not enough
        - instances where "which" is paired with a built in command which should create and error
        - testing redirection to see if it correctly outputs into files
        - testing piping with multiple different pipes/processes
        - testing piping with an error in one of the middle pipes, to see how it handles it going forward
        - tested wildcard by listing files that matched the wildcard description
        - attempting to change directories to go into a directory that does not exist
        - exiting the program at the end
