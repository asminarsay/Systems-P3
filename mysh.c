// for the outline:
// data structure to use:
    // create a struct that is a linked list/tree like structure that connects the parent process to its children
        // this struct also contains the unique process id 
    `   // and a variable that keeps track of if the process was a success ??
        // and a list of that node's child processes
            // put the list in order of the piping/redirection where the last index is the destination
        // terminate this list when done with those processes 

    // no need to create a list of parent nodes because only one parent node can exist at a time (mysh)
        // in the context of this project

// create a temporary list for the tokenizing part bc the tokenizing part just splits up
// the sentence so we know what command is being called
// so basically a string 


typedef struct process{
    char **env; // input information or string of action
    pid_t pid; // unique process id
    int success; // 0 if success 1 if not
    struct *next; // list of child nodes starts at the next node
    command_t command; // commands that was entered at this process
}process_t;

typedef struct command{
    char **strCommand; // command from terminal
    // definitely adding more or changing it around
}command_t;

