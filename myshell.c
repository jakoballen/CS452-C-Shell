/*
 * This code implements a simple shell program
 * It supports the internal shell command "exit",
 * backgrounding processes with "&", input redirection
 * with "<" and output redirection with ">".
 * However, this is not complete.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

extern char **getaline();

int ampersand(char **args);
int internal_command(char **args);
int do_command(char **args, int block, int input, char *input_filename, int output, char *output_filename, int append, char *append_filename, int shellSTDIN);
int redirect_input(char **args, char **input_filename);			   
int append_output(char **args, char **append_filename);
int redirect_output(char **args, char **output_filename);
void empty(char** a);
void parseArgs(char **args, int block, int input, char *input_filename, int output, char *output_filename, int append, char *append_filename, int shellSTDIN, int numArgs);
int findNextCommand(char **args, int i);
int size(char **a);
int pipeCount(char **args);
int pipes(char **args);

/*
 * Handle exit signals from child processes
 */
void sig_handler(int signal) {
  int status;
  int result = wait(&status);

  printf("Wait returned %d\n", result);
}

/*
 * The main shell function
 */
int main() {
  int i;
  char **args;
  int result;
  int block;
  int output;
  int input;
  int append;
  char *output_filename;
  char *input_filename;
  char *append_filename;
  int shellSTDIN = STDIN_FILENO;
  int numArgs = 0;

  // Set up the signal handler
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);	
	signal(SIGCHLD, sig_handler);

  // Loop forever
  while(1) {

    // Print out the prompt and get the input
    printf("->");
    args = getaline();
	
	numArgs = size(args);

    // No input, continue
    if(args[0] == NULL)
      continue;

    // Check for internal shell commands, such as exit
    if(internal_command(args))
      continue;

    // Check for an ampersand
    block = (ampersand(args) == 0);
	
    // Check for redirected input
    input = redirect_input(args, &input_filename);

    switch(input) {
    case -1:
      printf("Syntax error!\n");
      continue;
      break;
    case 0:
      break;
    case 1:
      printf("Redirecting input from: %s\n", input_filename);
      break;
    }

    // Check for append output
    append = append_output(args, &append_filename);

    switch(append) {
    case -1:
      printf("Syntax error!\n");
      continue;
      break;
    case 0:
      break;
    case 1:
      printf("Appending output to: %s\n", append_filename);
      break;
    }

    // Check for redirected output
    output = redirect_output(args, &output_filename);

    switch(output) {
    case -1:
      printf("Syntax error!\n");
      continue;
      break;
    case 0:
      break;
    case 1:
      printf("Redirecting output to: %s\n", output_filename);
      break;
    }

    // Parse args array for special symbols and do the commands from there
	parseArgs(args, block, input, input_filename, output, output_filename, append, append_filename, shellSTDIN, numArgs);
  }
}

/*
 * Check for ampersand as the last argument
 */
int ampersand(char **args) {
  int i;

  for(i = 1; args[i] != NULL; i++) ;

  if(args[i-1][0] == '&') {
    free(args[i-1]);
    args[i-1] = NULL;
    return 1;
  } else {
    return 0;
  }

  return 0;
}

/*
 * Check for internal commands
 * Returns true if there is more to do, false otherwise
 */
int internal_command(char **args) {
  if(strcmp(args[0], "exit") == 0) {
    exit(0);
  }

  return 0;
}

/*
 * Do the command
 */
int do_command(char **args, int block, int input, char *input_filename, int output, char *output_filename, int append, char *append_filename, int shellSTDIN) {

	int result;
	pid_t child_id;
	int status;


	/**	Part of pipe implementation
	int isPipe;
	int numberOfPipes;
	//Check for pipes
	isPipe = pipes(args);
	if(isPipe){
		numberOfPipes = pipeCount(args);
		
		int fd[2 * numberOfPipes];		//each pipe needs two file descriptors
		//set up fd for each pipe
		int i;
 		for(i = 0; i < numberOfPipes; i++) {
			pipe(fd + i*2);
		} 
	}
	*/
	
	// Fork the child process
	child_id = fork();

  // Check for errors in fork()
  switch(child_id) {
  case EAGAIN:
    perror("Error EAGAIN: ");
    return;
  case ENOMEM:
    perror("Error ENOMEM: ");
    return;
  }

  if(child_id == 0) {	// if is child
	//remove child from parent process group
	
	setpgid(child_id, child_id);	
    // Set signals in the child process
    signal(SIGTTOU, SIG_DFL);
    
    // Set up redirection in the child process
    if(input)
      freopen(input_filename, "r", stdin);

    if(output)
      freopen(output_filename, "w+", stdout);

    if(append)
      freopen(append_filename, "a", stdout);

    // Execute the command
    result = execvp(args[0], args);

    exit(-1);
  }

  // Wait for the child process to complete, if necessary
  if(block) {
	printf("Waiting for child, pid = %d\n", child_id);
	tcsetpgrp(shellSTDIN, child_id);			//give child control
	result = waitpid(child_id, &status, 0);		//wait for the child to finish and keep result
	tcsetpgrp(shellSTDIN, getpid());			//give control back to shell
	return WEXITSTATUS(status);					//return the exit status of the child
  }
}

//Process the args array to be able to handle special symbols
void parseArgs(char **args, int block, int input, char *input_filename, int output, char *output_filename, int append, char *append_filename, int shellSTDIN, int numArgs){
	
	char **parsedArgs = malloc((numArgs+1)*sizeof(char*));		//used to temporarily store arguments
	int cmdExitStatus;											//exit status of the command run in do_command
	int parsedArgsIndex = 0;									//next open index in parsedArgs
	int skipCmd = 0;											//indicated to skip next command
	char* lastSymbol;											//help to determine if skip is necessary
	
	int i;
	for(i = 0; args[i] != NULL; i++){
		if(args[i][0] == '('){									//check for open parenthesis
			if(skipCmd){
				i = findNextCommand(args, i);					//move to the next siginificant index
			}else{
				parsedArgsIndex = 0;							//first empty index is 0 now
				empty(parsedArgs);								//clear array after command has been run
			}
		}else if(args[i][0] == ';'){							//check for semicolon
			if(*parsedArgs != '\0' && !skipCmd){
				cmdExitStatus = do_command(parsedArgs, block, input, input_filename, output, output_filename, append, append_filename, shellSTDIN);
			}else{
				skipCmd = 0;									//false
			}
			parsedArgsIndex = 0;								//first empty index is 0 now
			empty(parsedArgs);									//clear array after command has been run
		} else if(args[i][0] == ')') {							//check for close parenthesis
			if(*parsedArgs != '\0' && !skipCmd) {
				cmdExitStatus = do_command(parsedArgs, block, input, input_filename, output, output_filename, append, append_filename, shellSTDIN);
			}
			parsedArgsIndex = 0;								//first empty index is 0 now
			empty(parsedArgs);									//clear array after command has been run
		}else if (args[i][0] == '&' && args[i+1][0] == '&'){	//check for double ampersand
			if(*parsedArgs != '\0' && !skipCmd){
				lastSymbol = "&&";								//keep track of symbol found
				skipCmd = ((cmdExitStatus = do_command(parsedArgs, block, input, input_filename, output, output_filename, append, append_filename, shellSTDIN) != 0)) ? 1 : 0;	//skip next command if first fails
				parsedArgsIndex = 0;							//first empty index is 0 now
				empty(parsedArgs);								//clear array after command has been run
				++i; 											// skip &&
			}
		}else if(args[i][0] == '|' && args[i+1][0] == '|') {	//check for double pipe
			if(*parsedArgs != '\0' && !skipCmd){
				lastSymbol = "||";								//keep track of symbol found
				skipCmd = ((cmdExitStatus = do_command(parsedArgs, block, input, input_filename, output, output_filename, append, append_filename, shellSTDIN) == 0)) ? 1 : 0;	//skip next command if first succeeds
				parsedArgsIndex = 0;							//first empty index is 0 now
				empty(parsedArgs);								//clear array after command has been run
				++i; 											// skip ||
			}
		}else{													//string at args[i] is not a special symbol and can just be added to the array
			parsedArgs[parsedArgsIndex] = args[i];
			++parsedArgsIndex;
			if(*(args+i+1) == NULL){							//is the last element of the array
				if(*parsedArgs != NULL && !skipCmd){			//another command had been found and is not supposed to be skipped
					do_command(parsedArgs, block, input, input_filename, output, output_filename, append, append_filename, shellSTDIN);
				}
			}
		}
	}
	parsedArgs = NULL;	//set pointer to null
}

/*
 * Check for input redirection
 */
int redirect_input(char **args, char **input_filename) {
  int i;
  int j;

  for(i = 0; args[i] != NULL; i++) {

    // Look for the <
    if(args[i][0] == '<') {
      free(args[i]);

      // Read the filename
      if(args[i+1] != NULL) {
	       *input_filename = args[i+1];
      } else {
	       return -1;
      }

      // Adjust the rest of the arguments in the array
      for(j = i; args[j-1] != NULL; j++) {
	       args[j] = args[j+2];
      }

      return 1;
    }
  }

  return 0;
}

//Check args for pipes
int pipes(char **args) {
	//iterates through args array, same way that the "ampersand" function does. 
	//Checks for first instance of '|' and it indicates that pipes are present.]
	int i;
	for(i = 1; args[i] != NULL; i++) {
		if(args[i][0] == '|' && args[i+1][0] != '|') {	//Makes sure to not count a '||' as a pipe
			return 1;
		}
	}
	return 0;
}

//iterates through args array, same way that the "ampersand" and "pipesExist" functions do. 
//Checks for every instance of the character '|' and adds to a total.
int pipeCount(char **args){
	int numberOfPipes = 0;
	int i;
	for(i = 1; args[i] != NULL; i++) {
		if(args[i][0] == '|' && args[i+1][0] != '|') {	//Makes sure to not count a '||' as a pipe
			numberOfPipes++;
		}else{
			continue;
		}
	}
	return numberOfPipes;
}

/*
 * Check for appended output
 */
int append_output(char **args, char **append_filename) {
  int i;
  int j;

  for(i = 0; args[i] != NULL; i++) {

    // Look for the >>
    if(args[i][0] == '>' && args[i+1][0] == '>') {
      free(args[i]);
      free(args[i+1]);

      // Get the filename
      if(args[i+2] != NULL) {
	        *append_filename = args[i+2];
      } else {
	        return -1;
      }

      // Adjust the rest of the arguments in the array
      for(j = i; args[j-1] != NULL; j++) {
	       args[j] = args[j+3];
      }

      return 1;
    }
  }

  return 0;
}

/*
 * Check for output redirection
 */
int redirect_output(char **args, char **output_filename) {
  int i;
  int j;

  for(i = 0; args[i] != NULL; i++) {

    // Look for the >
    if(args[i][0] == '>') {
      free(args[i]);

      // Get the filename
      if(args[i+1] != NULL) {
	*output_filename = args[i+1];
      } else {
	return -1;
      }

      // Adjust the rest of the arguments in the array
      for(j = i; args[j-1] != NULL; j++) {
	args[j] = args[j+2];
      }

      return 1;
    }
  }

  return 0;
}

//Move to index after the closing parenthesis
int findNextCommand(char **args, int i){
	while(args[i][0] != ')'){	//assuming there is a closing parenthesis
		i++;
	}
	return i + 1;

}

// Fill the array with null
void empty(char **a){
	int i;
	for(i = 0; a[i] != NULL; i++){
		a[i] = NULL;
	}
}

//Find the size of the array supplied
int size(char **a){
	int i;
	while(a[i] != NULL){	//count the number of arguments supplied
		++i;
	}
	return i;
}
