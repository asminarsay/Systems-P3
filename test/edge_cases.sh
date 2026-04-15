# --- 1. Basic Commands & Paths ---
ls
pwd
echo "Hello from mysh"
/bin/date

# --- 2. Built-in Functionality ---
which ls
which pwd
pwd
which non_existent_cmd

# --- 3. Redirection (Single Commands) ---
# Create a file and check content
echo "redirection test" > out.txt
cat out.txt
# Append or overwrite test
echo "new line" > out.txt
cat out.txt
# Input redirection
cat < out.txt

# --- 4. Piping (The Stress Test) ---
# Simple pipe
ls | wc -l
# Pipe with built-in
which cat | wc -c
# Long chain
echo 1 2 3 4 5 | cat | cat | wc -w

# --- 5. Wildcard Expansion ---
# Should expand to all .c files
ls *.c
# Should expand to all .h files (if they exist)
ls *.h
# Test no-match (should print literal string)
echo *.impossible_file_extension

# --- 6. Edge Cases & Whitespace ---
    ls    -l    
echo "    spaces    "
pwd;ls

# --- 7. Error Handling ---
# These should print errors to stderr but NOT crash the shell
fakecommand
cd /path/to/nowhere
which

# --- 8. Exit ---
exit