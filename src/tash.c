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
#include <fcntl.h>

//Max string size
#define STRING_MAX 20000
//Max number of commands/arguments in one line supported
#define MAXCMD 5000
//Delimiters for parsing
#define DELIM " \n\t"
//Number of items to increment to size of tokens by
#define TOKENS_SIZE 16
//Number of builtin functions
#define NUM_BUILT_INS 3
//Maximum number of paths in PATH
#define PATH_MAX 100
//Maximum number of parallel commands
#define MAX_PARALLEL 200

//PATH variable with support for up to PATH_MAX number of paths
static char **PATH;

//Function Prototypes
void batchMode(FILE *file);
void interactiveMode();
char **parseParallel(char *l);
char **parse(char *l);
void tashParallelHelp(char ***cmds);
int tashExecuteHelp(char **args);
int tashParallelExecute(char **args);
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




int main(int argc, char *argv[]) {
	//Malloc PATH
	PATH = (char **)malloc(sizeof(char *) * PATH_MAX);
	int i;
	//Malloc all entries in PATH
	for(i = 0; i < PATH_MAX; i++) {
		PATH[i] = (char *)malloc(sizeof(char) * STRING_MAX);
	}

	//Set default path
	char *a[2];
	char *b = "path";
	char *c = "/bin";
	a[0] = b; a[1] = c;
	path(a);
	//Check if run in batch mode, if so execute all commands in batch file then terminate, else run in interactive mode
	if(argc > 1) {
		//Batch Mode
		if(argc > 2) {
			//Too many arguments
			char error_message[30] = "An error has occurred\n";
			write(STDERR_FILENO, error_message, strlen(error_message));
			exit(1);
		}
		FILE *file = fopen(argv[1], "r");
		batchMode(file);

	} else {	
		//Interactive Mode
		interactiveMode();
	}
	return 1;
}

//Helpers
void batchMode(FILE *file) {
	char *line = (char *)malloc(sizeof(char) * STRING_MAX);
	//Infinite loop to read through batch file
	while(1) {
		//Grab input
		fgets(line, STRING_MAX, file);

		//Check for EOF
		if(feof(file)) { exit(0); }

		//Parse input for parallel commands
		char **parallel;
		parallel = parseParallel(line);
		if(parallel[1] != NULL) {
			//Parse and run commands in parallel
			int i;
			//Store parsed parallel commands in a list to pass to tashParallelHelp()
			char ***parsedParallel = (char ***)malloc(sizeof(char **) * MAX_PARALLEL);
			for(i = 0; parallel[i] != NULL; i++) {
				char **parsed = parse(parallel[i]);
				parsedParallel[i] = parsed;
			}
			tashParallelHelp(parsedParallel);
		} else {
			//Parse and run commands not in parallel
			//Parse input
			char **parsed;
			parsed = parse(parallel[0]);
			
			//Redirection error checking
			if(parsed != NULL) {
				//Run command not in parallel
				tashExecuteHelp(parsed);
			}
		}
	}
}







void interactiveMode() {
	char *line = NULL;
	size_t length = 0;
	//Interactive infinite loop, exit case is the exit builtin command
	while(1) {
		//Grab input
		printf("tash> ");
		getline(&line, &length, stdin);
		
		//Check for EOF
		if(feof(stdin)) { exit(0); }

		//Parse input for parrallel
		char **parallel;
		parallel = parseParallel(line);
		if(parallel[1] != NULL) {
			//Parse and run commands in parallel
			int i;
			//Store parsed parallel commands in a list to pass to tashParallelHelp()
			char ***parsedParallel = (char ***)malloc(sizeof(char **) * MAX_PARALLEL);
			for(i = 0; parallel[i] != NULL; i++) {
				char **parsed = parse(parallel[i]);
				parsedParallel[i] = parsed;
			}
			tashParallelHelp(parsedParallel);
		} else {
			//Parse and run commands not in parallel
			//Parse input
			char **parsed;
			parsed = parse(parallel[0]);
			
			//Redirection error checking
			if(parsed != NULL) {
				//Run command not in parallel
				tashExecuteHelp(parsed);
			}
		}
	}
}

//Parse parallel functions
char **parseParallel(char *l) {
	//Tracks the current max number of elements in tokens
	int tSize = TOKENS_SIZE;
	//holds all parallel commands from l
	char **commands = malloc(tSize * sizeof(char*));
	//used to grab individual commands with strtok()
	char *command;
	//tracks the current index
	int i = 0;
	//tracker for error checking where there is only "&"
	int parallelPresent = 0;
	if(strchr(l, '&') != NULL) {
		parallelPresent = 1;
	}

	//grab first command
	command = strtok(l, "&");
	while(command != NULL && strcmp(command, "\n") != 0) {
		//Store the command into commands
		commands[i] = command;
		i++;
		
		//Grab next token
		command = strtok(NULL, "&");
		//If commands is full, increase its size
		if(i > tSize) {
			tSize += TOKENS_SIZE;
			commands = realloc(commands, tSize * sizeof(char*));
		}

	}
	//Set the last command to NULL
	commands[i] = NULL;

	//Error when input is only "&"'s
	if((commands[0] == NULL || strcmp(commands[0], "\n") == 0) && parallelPresent) {
		char error_message[30] = "An error has occurred\n";
		write(STDERR_FILENO, error_message, strlen(error_message));

	}
	return commands;
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
	//Save pointers for reentry with strtok_r
	char *save1, *save2;
	//Trackers for error when there is too many ">" or files after that
	int redirErr = 0;
	int redirFileErr = 0;

	//grab first token
	token = strtok_r(l, DELIM, &save1);
	//iterate through all tokens
	while(token != NULL) {
		//Redirection error checking
		if(redirErr && redirFileErr) {
			//Should be empty, thus error
			char error_message[30] = "An error has occurred\n";
			write(STDERR_FILENO, error_message, strlen(error_message));
			return NULL;
		} else if(redirErr) {
			//Redirect operator found, output file is next token
			redirFileErr = 1;
		}

		//Redirection checking and splitting
		int j;
		int found = 0;
		for(j = 0; token[j] != '\0'; j++) {
			//If redirection char is found then set found to 1
			//and exit loop
			if(token[j] == '>') {
				found = 1;
				break;
			}
		}
		//If redirection character was found...
		if(found) {
			if(redirErr || redirFileErr) {
				char error_message[30] = "An error has occurred\n";
				write(STDERR_FILENO, error_message, strlen(error_message));
				return NULL;
			}
			redirErr = 1;
			//Holds the current token
			char *temp;
			//Redirection character is the first character
			if(j == 0) {
				//Add ">" to tokens
				tokens[i] = ">";
				i++;
				//Deliminate on ">"
				temp = strtok_r(token, ">", &save2);
				if(temp != NULL) {
					redirFileErr = 1;
					tokens[i] = temp;
					i++;
				}
			}
			//Redirection character is not the first character
			if(j > 0) {
				//Deliminate on ">"
				temp = strtok_r(token, ">", &save2);
				tokens[i] = temp;
				i++;
				//Add ">" to tokens
				tokens[i] = ">";
				i++;
				//If next token is NULL, then ">" was at the end
				temp = strtok_r(NULL, ">", &save2);
				if(temp != NULL) {
					redirFileErr = 1;
					tokens[i] = temp;
					i++;
				}
			}
		} else {
			//Redirection character wasnt found
			//Store the token into tokens
			tokens[i] = token;
			i++;
		}
		//Grab next token
		token = strtok_r(NULL, DELIM, &save1);
		
		//If tokens is full, increase its size
		if(i > tSize) {
			tSize += TOKENS_SIZE;
			tokens = realloc(tokens, tSize * sizeof(char*));
		}
	}
	//Redirection error checking
	if(redirErr && !redirFileErr) {
		//Redirection operator found, but no file following it
		char error_message[30] = "An error has occurred\n";
		write(STDERR_FILENO, error_message, strlen(error_message));
		return NULL;
	}

	//Set the last argument to null for execv
	tokens[i] = NULL;
	return tokens;

}

//Parallel execution helper function, determines if a function is empty, a builtin, or not,
//and after all commands are executed, it waits for all of them to finish
void tashParallelHelp(char ***cmds) {
	//Iterate through all commands
	int waitingProcesses = 0;
	int pidList[MAX_PARALLEL];
	int i;
	for(i = 0; cmds[i] != NULL; i++) {
		char **temp = cmds[i];
		//check for empty command
		if(temp[0] == NULL) { /* do nothing */ }
		//check if command is a builtin
		int j;
		int builtIn = 0;
		for(j = 0; j < NUM_BUILT_INS; j++) {
			if(strcmp(temp[0], builtIns[j]) == 0) {
				(*builtInPointers[j])(temp);
				builtIn = 1;
				break;
			}
		}
		//command is not a builtin so execute it
		if(!builtIn) {
			int pid = tashParallelExecute(temp);
			pidList[waitingProcesses] = pid;
			waitingProcesses++;
		}
	}
	//Wait on all processes to finish
	for(i = 0; i < waitingProcesses; i++) {
		int status;
		waitpid(pidList[i], &status, 0);
	}
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

//Parallel execute function for non-builtin commands
int tashParallelExecute(char **args) {
	pid_t pid;
	//fork and keep track of pid
	pid = fork();
	//check for forking error
	if(pid < 0) {
		char error_message[30] = "An error has occurred\n";
		write(STDERR_FILENO, error_message, strlen(error_message));
	} else if(pid == 0) {
		//if child process, then find command location in PATH and execute
		char p[STRING_MAX];
		strcpy(p, PATH[0]);
		int i;
		//if p is null or empty, then error
		if(p == '\0' || strcmp(p, "") == 0) {
			char error_message[30] = "An error has occurred\n";
			write(STDERR_FILENO, error_message, strlen(error_message));
			exit(EXIT_FAILURE);
		}
		//p must contain something, iterate until program is found or
		//until p contains "" or is NULL
		for(i = 1; strcmp(p, "") != 0 && p != NULL; i++) {
			char *temp = "/";
			strcat(p, temp);
			strcat(p, args[0]);
			int a = access(p, X_OK);
			
			//access confirmed that address p contains the program so execute it
			if(a == 0) {
				//Check for redirection
				int j;
				char **realArgs = (char **)malloc(sizeof(char *) * MAXCMD);
				for(j = 0; args[j] != NULL; j++) {
					//If Redirection symbol found, redirect child's output to output file
					//and break from loop
					if(strcmp(args[j], ">") == 0) {
						//Open the output file
						FILE *out = fopen(args[j+1], "w");
						//Redirect stdout and stderr
						dup2(fileno(out), STDOUT_FILENO);
						dup2(fileno(out), STDERR_FILENO);
						//close the original fd
						fclose(out);
						
						break;
					} else {
						realArgs[j] = args[j];
					}
				}


				if(execv(p, realArgs) == -1) { 
					char error_message[30] = "An error has occurred\n";
					write(STDERR_FILENO, error_message, strlen(error_message));
				}
			}
			strcpy(p, PATH[i]);
		}
		//if the child reaches this point, then it failed to execute a command
		char error_message[30] = "An error has occurred\n";
		write(STDERR_FILENO, error_message, strlen(error_message));
		exit(EXIT_FAILURE);
	} else {
		//parent does nothing,
		//parent waits in tashParallelHelp
	}

	return pid;
}


//Execute function for non-builtin commands
int tashExecute(char **args) {
	pid_t pid;
	//fork and keep track of pid
	pid = fork();
	//check for forking error
	if(pid < 0) {
		char error_message[30] = "An error has occurred\n";
		write(STDERR_FILENO, error_message, strlen(error_message));
	} else if(pid == 0) {
		//if child process, then find command location in PATH and execute
		char p[STRING_MAX];
		strcpy(p, PATH[0]);
		int i;
		//if p is null or empty, then error
		if(p == '\0' || strcmp(p, "") == 0) {
			char error_message[30] = "An error has occurred\n";
			write(STDERR_FILENO, error_message, strlen(error_message));
			exit(EXIT_FAILURE);
		}
		//p must contain something, iterate until program is found or
		//until p contains "" or is NULL
		for(i = 1; strcmp(p, "") != 0 && p != NULL; i++) {
			char *temp = "/";
			strcat(p, temp);
			strcat(p, args[0]);
			int a = access(p, X_OK);
			
			//access confirmed that address p contains the program so execute it
			if(a == 0) {
				//Check for redirection
				int j;
				char **realArgs = (char **)malloc(sizeof(char *) * MAXCMD);
				for(j = 0; args[j] != NULL; j++) {
					//If Redirection symbol found, redirect child's output to output file
					//and break from loop
					if(strcmp(args[j], ">") == 0) {
						//Open the output file
						FILE *out = fopen(args[j+1], "w");
						//Redirect stdout and stderr
						dup2(fileno(out), STDOUT_FILENO);
						dup2(fileno(out), STDERR_FILENO);
						//close the original fd
						fclose(out);
						
						break;
					} else {
						realArgs[j] = args[j];
					}
				}


				if(execv(p, realArgs) == -1) { 
					char error_message[30] = "An error has occurred\n";
					write(STDERR_FILENO, error_message, strlen(error_message));
				}
			}
			strcpy(p, PATH[i]);
		}
		//if the child reaches this point, then it failed to execute a command
		char error_message[30] = "An error has occurred\n";
		write(STDERR_FILENO, error_message, strlen(error_message));
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
	//check if exit was passed an argument, and output an error if so
	if(args[1] != NULL) { 
		char error_message[30] = "An error has occurred\n";
		write(STDERR_FILENO, error_message, strlen(error_message));
		return 1;
	}
	exit(EXIT_SUCCESS);
	return 1;
}

//cd command, changes to current directory to d, outputs an error if it failed
int cd(char **args) {
	if(args[1] == NULL) {
		char error_message[30] = "An error has occurred\n";
		write(STDERR_FILENO, error_message, strlen(error_message));
	}
	else {
		if(chdir(args[1]) != 0) {
			char error_message[30] = "An error has occurred\n";
			write(STDERR_FILENO, error_message, strlen(error_message));
		}
	}
	return 1;
}

//path command
int path(char **args) {
	//Empty PATH of previous paths
	int i;
	for(i = 0; PATH[i] != NULL; i++) {
		//Empty the string
		strcpy(PATH[i], "");
	}

	//Parse through the paths until the end, if the last character is a '/', then remove it
	//Start at args[1] because args[0] contains "path"
	for(i = 1; args[i] != NULL; i++) {
		int j;
		char *temp = args[i];
		char secondToLast = '\0';
		//iterate through the string until the end
		for(j = 0; temp[j] != '\0'; j++) { secondToLast = temp[j]; }
		//if the secondToLast is a '/', then remove it
		if(secondToLast == '/') { temp[j-1] = '\0'; }

		//Add the corrected path to PATH
		strcpy(PATH[i-1], temp);

	}
	return 1;
}






