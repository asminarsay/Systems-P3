# Changes Made to mysh.c

## Prompt format
The prompt was only showing ~ when exactly at the home directory. If you were in a subdirectory like /home/user/subdir it would just show the full path instead of ~/subdir. We switched from strcmp to strncmp so it checks if the cwd starts with the home directory path, and if it does it swaps that prefix out for ~. Also added a guard so /home/user doesnt accidentally match /home/username by checking the next character is / or null.

## Exit status reporting
Nothing was being printed after a command finished. The spec says if a command exits with a non-zero code we need to print "Exited with status N" and if it got killed by a signal we print "Terminated by signal". We changed bare_names, exec_redirect, builtin_redirect, and apply_piping to all return int instead of void so they pass back the raw waitpid status. built_in returns 0 for success and 1 for failure. Then at the bottom of the main loop we decode the status with WIFEXITED/WEXITSTATUS/WIFSIGNALED/WTERMSIG and print the right message. Only does this in interactive mode.

## Exit command handling
built_in was calling exit(0) directly which killed the whole process immediately, so the goodbye message at the end of main never printed. It was also printing "Exiting my shell!" even in batch mode. We moved exit detection into main itself, before it ever calls built_in. Now it just sets should_exit = 1 and the loop ends naturally, which lets the goodbye message at the bottom of main run like its supposed to. The exit(0) stays in built_in but only gets hit when its called from a child process in a pipe or redirect. We also added exit detection in apply_piping so commands like foo | exit properly terminate the shell after the pipeline finishes.

## Batch mode /dev/null stdin
Only apply_redirection was pointing child stdin to /dev/null in batch mode. bare_names was forking and execing without it, so something like cat in a batch script would try to read from the terminal or steal bytes from the command stream. We added the /dev/null redirect in both fork paths inside bare_names (the path-based one and the bare name one). Also added it in apply_piping for the first process in a pipeline since that one doesnt get its stdin from a pipe.

## Crash fix for empty commands after redirection
If someone typed something like > file with no actual command, parse_redirection would strip out the > and filename leaving an empty token list. Then main would try to strcmp on tokens[0] which was NULL and crash. Added a check for expanded.count == 0 right after redirection parsing so it just skips to the next command.

## which cleanup
which wasnt checking for too many arguments, only too few. Changed the check from tl->count == 1 to tl->count != 2 so it catches both cases. The spec says which should print nothing and fail on error so we removed all the error messages from the which handler. Also fixed a memory leak where bare_name_search returned a strdup'd path that was never freed.

## Dead code removal
There was an unreachable exit(0) in bare_names after an else block that already called exit(1). Removed it.

## Pipeline improvements
apply_piping was using wait(NULL) in a loop so we couldnt tell which status belonged to which process. Changed it to store each fork result in a pids array and use waitpid with specific PIDs. The status from the last process in the pipeline is what gets returned, which is what the spec requires. Also changed built_in calls in pipeline children to pass their return value to exit() instead of always exiting 0.

## Function return types
Changed bare_names, exec_redirect, builtin_redirect, built_in, and apply_piping from void to int. The forking functions return the raw waitpid status or -1 on error. built_in returns 0 for success and 1 for failure. apply_piping takes an extra int pointer parameter has_exit so it can tell main if exit was one of the sub-commands. This was needed for exit status reporting and exit detection to work.

## Added signal.h
Needed for strsignal() which we use to print signal names in the exit status messages.

## Output buffering fix
Built-in commands like pwd and which use printf which goes through stdios buffer. In batch mode stdout isnt a terminal so stdio uses full buffering, which meant printf output would pile up and come out all at once at the end instead of in the right order between external commands. Added fflush(stdout) at the top of the main loop so any buffered output from the previous command gets flushed before the next one runs.

## Empty pipeline segment fix
If someone typed something like | echo hello or echo hello | | echo world, the empty segment between the pipes would have zero tokens. The child process for that segment would try to strcmp on NULL and segfault. Added a check in apply_piping so if a segment has count 0 the child just exits immediately instead of crashing.
