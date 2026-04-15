# Issues to Fix in mysh.c



## 2. Exit status reporting

**Location**: main loop, after command execution

**Current**: Nothing is printed after a command finishes.

**Spec**: "If the command exited normally with a non-zero exit code, mysh will print a message such as 'Exited with status N'. If the command was terminated by a signal, mysh will print a message such as 'Terminated by signal X'."

**Fix**: After every command execution in the main loop, decode the `waitpid` status using `WIFEXITED`/`WEXITSTATUS`/`WIFSIGNALED`/`WTERMSIG` and print the appropriate message. Only in interactive mode.

**Dependency**: Requires change #5 (functions must return the wait status).


## 3. Exit command handling

**Location**: built_in(), lines 423-426

**Current**:
```c
else if(strcmp(tl->tokens[0],"exit") == 0){
    printf("Exiting my shell!\n");
    exit(0);
}
```

**Problem**: `exit(0)` kills the process immediately — the goodbye message at the end of main() never runs. Also `printf` prints even in batch mode, but the spec says goodbye messages are only for interactive mode.

**Spec**: "after receiving the exit command or reaching the end of the input stream, it should print a message such as 'Exiting my shell.'" (interactive only). Also: "A command such as `foo | exit` will terminate mysh once foo is complete."

**Fix**: In main, check for `exit` before calling `built_in`. Set `should_exit = 1` and let the loop end naturally so the goodbye message at the bottom of main runs. Keep `exit(0)` inside `built_in` only for when it's called from a child process (pipe/redirect fork). Also scan pipeline sub-commands for `exit` and set the flag after the pipeline finishes.


## 4. Batch mode /dev/null stdin

**Location**: bare_names() and apply_piping()

**Current**: Only `apply_redirection()` redirects child stdin to `/dev/null` in batch mode. `bare_names()` just forks and execs without it. `apply_piping()` doesn't redirect the first process's stdin either.

**Spec**: "In batch mode, mysh will instead use /dev/null as the default input stream for child processes."

**Fix**: In `bare_names()`, after `fork()` in the child, open `/dev/null` and `dup2` it to stdin when `!interactive && !isPipe`. In `apply_piping()`, do the same for `i == 0` (first process in pipeline).


## 5. Function return types (void -> int)

**Location**: bare_names(), exec_redirect(), builtin_redirect(), built_in(), apply_piping()

**Current**:
- `bare_names` calls `waitpid(pid, NULL, 0)` — throws away status
- `exec_redirect` captures status but doesn't return it
- `builtin_redirect` — same
- `built_in` — no return value for success/failure
- `apply_piping` — uses `wait(NULL)`, can't tell which child's status belongs to whom

**Fix**: Change all to return `int`. `bare_names`/`exec_redirect`/`builtin_redirect`/`apply_piping` return the raw `waitpid` status. `built_in` returns 0 for success, 1 for failure. Main uses the return values for exit status reporting (#2).


## 6. Pipeline fixes

### a) PID tracking
**Location**: apply_piping(), lines 526-528

**Current**: Uses `wait(NULL)` in a loop, which waits for any child. Can't determine which status belongs to the last process.

**Fix**: Store each `fork()` result in a `pids[]` array, then `waitpid(pids[i], &status, 0)` for each. Keep the status from `i == num_pipes` (last process).

### b) Double-fork
**Location**: apply_piping(), line 515

**Current**: The pipeline child calls `bare_names(&split[i], 1)`, which forks again internally. Each pipeline stage creates two processes instead of one.

**Fix**: Resolve the path and call `execv` directly in the pipeline child instead of going through `bare_names`.

### c) which cleanup
**Location**: which_check() and built_in() which handler

**Current**:
- `which_check` loop always checks `tokens[1]` instead of `tokens[i]` (harmless but wrong)
- `which` doesn't check for `tl->count > 2` (too many args should fail)
- `which` leaks the `path` from `bare_name_search` (never freed)

**Fix**: Replace `which_check` with a simpler `is_builtin(name)` helper. Add `tl->count != 2` check. Free the path after printing.
