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
    The main method first checks for arguments first to determine which mode to present the shell in aka interactive or batch mode
    It if is interactive, we set an integer to keep track of this so we know if we should write the "cwd"$ to the command line.
    By reading a stream of bytes, it first runs our tokenizing methods, our wildcard expansion, and our redirection expansion.
    After running those, we have a tokenized command stored in our struct, which we can then parse to see which command type we are working with.
    If the lines presents a pipe, it calls the apply_piping function. 
    Then, we check to see if the line calls built in commands and if it does it calls that function, but if it does not it calls the bare_names function which handle both the path to the executable files (if it detects "/") and bare name commands