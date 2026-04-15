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