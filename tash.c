/*	Tristan Pior
 *	CS434.003
 *	Project 1
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

//Max string size
#define STRING_MAX 1000
//Max number of commands/arguments in one line supported
#define MAXCMD 100
//Delimiters for parsing
#define DELIM " \n\t"
//Number of items to increment to size of tokens by
#define TOKENS_SIZE 16
//Number of builtin functions
#define NUM_BUILT_INS 3

static char **PATH;

//Function Prototypes
void interactiveMode();
char **parse(char *l);
int tashExecuteHelp(char **args);
int tashExecute(char **args);
int exitBuiltin(char **args);
int cd(char **args);
int path(char **args);

//List of the commands for builtin functions
char *builtIns[] = {
	"exit",
	"cd",
	"path"
};



//List of the function pointers for their respective builtin commands
int (*builtInPointers[]) (char **) = {
	&exitBuiltin,
	&cd,
	&path

};




int main() {	
	//Set default path
	char *a[1];
	char *b = "/bin";
	a[0] = b;
	path(a);
	//Check if run in batch mode, if so execute all commands in batch file then terminate, else run in interactive mode

	//Batch Mode
	
	
	//Interactive Mode
	interactiveMode();
	
	return 1;
}

//Helpers
void interactiveMode() {
	char *line = NULL;
	size_t length = 0;
	//Interactive infinite loop, exit case is the exit builtin command
	while(1) {
		//Grab input
		printf("tash> ");
		getline(&line, &length, stdin);

		//Parse input
		char **parsed;
		parsed = parse(line);
		
		//Check for redirection

		//Check for parallel commands
		
		//Run commands in parrallel

		//Run command if not in parallel
		tashExecuteHelp(parsed);
		
		printf("TEST\n");		
		//Temporary exit condition
		//exit(EXIT_SUCCESS);
	}


}

//Parsing helper function, passes back an array of tokens
char **parse(char *l) {
	//Tracks the current max number of elements in tokens
	int tSize = TOKENS_SIZE;
	//holds all tokens from l
	char **tokens = malloc(tSize * sizeof(char*));
	//used to grab individual tokens with strtok()
	char *token;
	//tracks the current index
	int i = 0;

	//grab first token
	token = strtok(l, DELIM);
	//iterate through all tokens
	while(token != NULL) {
		//Store the token into tokens
		tokens[i] = token;
		i++;
		//Grab next token
		token = strtok(NULL, DELIM);

		//If tokens is full, increase its size
		if(i > tSize) {
			tSize += TOKENS_SIZE;
			tokens = realloc(tokens, tSize * sizeof(char*));
		}
	}
	//Set the last argument to null for execv
	tokens[i] = NULL;
	return tokens;

}

//Execute helper function, determines if the passed function is empty, a builtin, or not
int tashExecuteHelp(char **args) {
	//check for empty command
	if(args[0] == NULL) { return 1; }
	//check if command is a builtin
	int i;
	for(i = 0; i < NUM_BUILT_INS; i++) {
		if(strcmp(args[0], builtIns[i]) == 0) { return (*builtInPointers[i])(args); }
	}
	//command is not a builtin so execute it
	return tashExecute(args);
}

//Execute function for non-builtin commands
int tashExecute(char **args) {
	pid_t pid;
	//fork and keep track of pid
	pid = fork();
	//check for forking error
	if(pid < 0) {
		perror("Forking failed");
	} else if(pid == 0) {
		//if child process, then find command location in PATH and execute
		char p[STRING_MAX];
		strcpy(p, PATH[0]);
		int i;
		for(i = 1; p != NULL; i++) {
			char *temp = "/";
			strcat(p, temp);
			strcat(p, args[0]);
			int a = access(p, X_OK);
			//access confirmed that address p contains the program so execute it
			if(a == 0) {
				if(execv(p, args) == -1) { perror("Failed to execute"); }
			} else { perror("Access error"); }
			strcpy(p, PATH[i]);
		}
		//if the child reaches this point, then it failed to execute a command
		exit(EXIT_FAILURE);
	} else {
		//parent waits for the child process to finish
		int status;
		wait(&status);
	}

	return 1;
}







//Builtin Commands

//exit command, calls exit system call with code 0
int exitBuiltin(char **args) {
	exit(EXIT_SUCCESS);
	return 1;
}

//cd command, changes to current directory to d, outputs an error if it failed
int cd(char **args) {
	if(args[1] == NULL) { printf("Argument expected for cd"); }
	else {
		if(chdir(args[1]) != 0) { perror("chdir() failed"); }
	}
	return 1;
}

//path command
int path(char **args) {
	PATH = args;
	return 1;
}
