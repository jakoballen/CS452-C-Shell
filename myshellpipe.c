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
	int do_command(char **args, int block,
				   int input, char *input_filename,
				   int output, char *output_filename,
				   int append, char *append_filename, int shellSTDIN);
	int redirect_input(char **args, char **input_filename);			   
	int append_output(char **args, char **append_filename);
	int redirect_output(char **args, char **output_filename);
	bool pipesExist(char **args);
	int pipeCount(char **args);

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
	  bool isPipes;
	  
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

		//Check if there is a pipe. If there is, find out how many
		isPipes = false;
		int numberOfPipes;
		
		//iterates through args array, same way that the "ampersand" function does. 
		//Checks for first instance of '|' and it indicates that pipes are present.
		for(i = 1; args[i] != NULL; i++) {
			if(args[i][0] == '|' && args[i+1][0] != '|') {	//Makes sure to not count a '||' as a pipe
				isPipes = true;
			}else{
				continue;
			}
		}
		
		if(isPipes){
			numberOfPipes = pipeCount(args);
			//printf("Pipes: %i\n" , numberOfPipes);
			
		}
		
		int fd[2 * numberOfPipes];
		
 		for(i = 0; i < numberOfPipes; i++) {
			pipe(fd + i*2);
		} 
		
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

		// Do the command
		do_command(args, block,
			   input, input_filename,
			   output, output_filename,
			 append, append_filename, shellSTDIN);
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


	//iterates through args array, same way that the "ampersand" and "pipesExist" functions do. 
	//Checks for every instance of the character '|' and adds to a total.
	int pipeCount(char **args){
		int numberOfPipes = 0;
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
	 * Do the command
	 */
	int do_command(char **args, int block,
			   int input, char *input_filename,
			   int output, char *output_filename,
			 int append, char *append_filename, int shellSTDIN) {

	  int result;
	  pid_t child_id;
	  int status;
	 

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
		setpgid(child_id, child_id);	//remove child from parent process group

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
		tcsetpgrp(shellSTDIN, child_id);	//give child control
		result = waitpid(child_id, &status, 0);
		tcsetpgrp(shellSTDIN, getpid());	//give control back to shell
	  }
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
