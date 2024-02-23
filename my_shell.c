#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64
#define MAX_NUM_PROCESSES 64


//Array for running or zombie processes - stores pids 
static pid_t run_pid[MAX_NUM_PROCESSES]; 

//Array for foreground processes (Helps SIGINT handler) - stores pids 
static pid_t curr_pid[MAX_NUM_PROCESSES]; 

//Variable to stop sequential execution after SIGINT is signaled 
static int stop; 

//Variable to specify to the startCommand function whether the sequence of input commands is in parallel 
static int parallel; 

/* Splits the string by space and returns the array of tokens
*
*/
char **tokenize(char *line)
{
  char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
  char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
  long unsigned int i; 
  int tokenIndex = 0, tokenNo = 0;

  for(i = 0; i < strlen(line); i++){

    char readChar = line[i];

    if (readChar == ' ' || readChar == '\n' || readChar == '\t'){
      token[tokenIndex] = '\0';
      if (tokenIndex != 0){
		tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
		strcpy(tokens[tokenNo++], token);
		tokenIndex = 0; 
      }
    } 
	else {
      token[tokenIndex++] = readChar;
    }
  }
 
  free(token);
  tokens[tokenNo] = NULL ;
  return tokens;
}


/* Handler function to handle the Ctrl+C signal to not terminate shell process, but only running processes.
	Works by setting this parent process (aka the shell) to this handler when SIGINT is called, but all its 
	children will still use the same handler for the SIGINT call. */
void proccessTerm(){
	//Send to stop sequential execution function 
	stop = 1; 
    printf("\n");
}

/*Helper function to run through to reap and check if background processes have completed.
  When processes are completed shell prints the statement they have finished and update their position in pid array.*/
void checkRunningProc(){
	int status; 

	//Loop through array of running pids to see which ones to remove 
	for(int i = 0;i<MAX_NUM_PROCESSES;i++){
		//Check to see empty locations in array, pid could never be -1 or error already would have thrown
		if(run_pid[i] == -1) {  continue; }

		if(!waitpid(run_pid[i], &status, WNOHANG)) continue; //Proc still running or error 

		if(WIFEXITED(status) && WIFSIGNALED(status)) {
			printf("Shell: Incorrect Command\n"); 
			continue;
		}

		run_pid[i] = -1; 
		printf("Shell: Background process finished\n");	
	} 

}


/*Function to Fork child processes, executes these children to execute user commands
  and then wait for dead children to be reaped.
  @param takes in tokens to parse and then execute (if correct)
  @return 1 to maintain execution of the shell*/
int exec_command(char **args, int foreground)
{
	//Variable to store PIDs 
	pid_t pid; 
	//Variable to track the status of process
	int status; 

	//Fork child 
	pid = fork();

	//Check for child PID
	if(pid == 0){
		//Exec the command and check value for no -1
		if(execvp(args[0], args) == -1) { printf("Shell: Incorrect Command\n"); exit(EXIT_FAILURE);}
	}
	//Error in creating the fork
	else if(pid < 0) { perror("Error "); }


	//Parent process should wait (fg) or not (bg)
	if (foreground){
		//Drop this current pid into the array to track 
		for(int i = 0;i<MAX_NUM_PROCESSES;i++){
			//Place pid at valid slot and break out of function, at the next -1 
			if(curr_pid[i] == -1) {
				curr_pid[i] = pid;  
				break; 
			}
		}

		do{
			waitpid(pid, &status, WUNTRACED);
		} while (!WIFEXITED(status) && !WIFSIGNALED(status)); 
	}
	else{
		//Drop this runnning pid into the array to track 
		for(int i = 0;i<MAX_NUM_PROCESSES;i++){
			//Place pid at valid slot and break out of function, at the next -1 
			if(run_pid[i] == -1) {
				run_pid[i] = pid; 
				break; 
			}
		}
		//Don't have to have parent wait on child
		//Set new process group 
		if(setpgid(pid,0)==-1) perror("Error ");
	}

	return 1; 

}

/*Function for running built-in cd command 
  @param takes in tokens to parse and then execute (if correct), cd is args[0] and directory is args[1] 
  @return 1 to maintain execution of the shell*/
int cd_command(char **args){
	if (args[1] == NULL) printf("Syntax not supported please enter a path or .. to go to parent directory.\n");
	else{
		if(chdir(args[1]) != 0) perror("Error "); 
	}
	return 1; 
}

/*Function for running built-in exit command. 
  Kill each background process, free tokens, and
  exit 0 to terminate execution of the shell.*/
int exit_command(char **tokens){
	int i;
	//Use PID table of current running processes to kill them
	for(i=0;run_pid[i] != -1;i++){
		kill(run_pid[i],SIGKILL); 
	}

	// Freeing the allocated memory	
	for(i=0;tokens[i]!=NULL;i++){
		free(tokens[i]);
	}

	free(tokens);
	
	exit(0); 
}

/*Helper function for the parallel execution to run processes like background in exec_command
  except has to wait for every parallel process to terminate before finishing like foreground*/
int pll_exec_command(char **args){
	//Variable to store PIDs 
	pid_t pid; 

	//Fork child 
	pid = fork();

	//Check for child PID
	if(pid == 0){
		//Exec the command and check value for no -1
		if(execvp(args[0], args) == -1) { printf("Shell: Incorrect Command\n"); exit(EXIT_FAILURE); }
	}
	//Error in creating the fork
	else if(pid < 0) { perror("Error "); }

	//Drop this current pid into the array to track 
		for(int i = 0;i<MAX_NUM_PROCESSES;i++){
			//Place pid at valid slot and break out of function, at the next -1 
			if(curr_pid[i] == -1) {
				curr_pid[i] = pid;
				break; 
			}
		}

	return 1; 
}

/*Function to check to run a built-in command or not built-in
   @param tokens is the array of input tokens and foreground is a check for running command in fg or bg
   @returns process it runs */
int startCommand(char **tokens, int foreground){

		//Executing commands, either cd or close, or parallel vs other executions 
		if (strcmp(tokens[0], "cd") == 0) return cd_command(tokens);
		else if (strcmp(tokens[0], "exit") == 0) return exit_command(tokens); 
		else if (parallel) { return pll_exec_command(tokens); } 
		else return exec_command(tokens,foreground); 
}

/*Helper function to let parallel execution wait till all proc finish until it returns to shell. 
  Acts much like check running process, except now waits on waitpid. */
void pll_wait(){
		//Variable to track the status of process
		int status; 

		for(int i = 0;i<MAX_NUM_PROCESSES;i++){
		//Check to see empty locations in array, pid could never be -1 or error already would have thrown
		if(curr_pid[i] == -1) {  
			continue; 
		}

		if(WIFEXITED(status) && WIFSIGNALED(status)) {
			printf("Shell: Incorrect Command\n"); 
			continue;
		}
		while(!waitpid(curr_pid[i], &status, WNOHANG)) { } //Proc still running, wait for it to finish 

		curr_pid[i] = -1; 
	}
}

/*Function to handle the &&& operator between commands. Works in parallel execution, all commands run at once. 
  @param tokens is the array of tokens parsed
  @return 1 after to continued execution of shell, wheteher commands were valid or not*/
int pll_exec(char **tokens){
	//Temp array to store tokens for each command between the && and run each indvidually 
	char **temp = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));

	//Variable to store current index of 'start' of command, either begining of tokens or right after a &&
	//Allows for easy integration into exec_command
	int curr_start = 0; 
	//Parallel uses it's own function this is just for passing into startCommand 
	int foreground = 1; 
	//Set to parallel mode 
	parallel = 1; 
	//Counters for loops
	int i;
	int j; 

	for(i=0;tokens[i]!=NULL;i++){
		if((strcmp(tokens[i], "&&&") == 0) || (tokens[i+1] == NULL)) {
			//Case for last command before end of string, want to run this one in the foreground
			if (tokens[i+1] == NULL) { temp[i - curr_start] = tokens[i]; }

			//Run the command currently stored in temp 
			startCommand(temp,foreground);

			//Clear the temp array to begin filling for next command
			for(j=0;temp[j]!=NULL;j++){
				temp[j] = '\0';
			}

			//Update new start, only needed for when not at end of array (aka last command in sequence)
			if (tokens[i+1]!=NULL) curr_start = i+1; 
			
		}
		//Adding command to temp array 
		else { temp[i - curr_start] = tokens[i]; }
	}
	//Wait for all parallel processes to be done running before returning
	pll_wait(); 
	
	//Free temporary array
	free(temp); 
	return 1; 
}

/*Function to handle the && operator between commands. Works in sequence execution, commands run left to right (one after the other).
  @param tokens is the array of tokens parsed
  @return 1 after to continued execution of shell, wheteher commands were valid or not*/
int seq_exec(char **tokens){
	//Temp array to store tokens for each command between the && and run each indvidually 
	char **temp = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));

	//Variable to store current index of 'start' of command, either begining of tokens or right after a &&
	//Allows for easy integration into exec_command
	int curr_start = 0; 
	int foreground = 1; 
	//Counters for loops
	int i;
	int j; 

	for(i=0;tokens[i]!=NULL;i++){

		//For when SIGINT is passed, breaking the function and not running anymore commands
		if(stop) { break; } 

		if((strcmp(tokens[i], "&&") == 0) || (tokens[i+1] == NULL)) {
			//Case for last command before end of string
			if (tokens[i+1] == NULL) {
				temp[i - curr_start] = tokens[i]; }
			//Run the command currently stored in temp
			startCommand(temp,foreground);  

			//Clear the temp array to begin filling for next command
			for(j=0;temp[j]!=NULL;j++){
				temp[j] = '\0';
			}

			//Update new start, only needed for when not at end of array (aka last command in sequence)
			if (tokens[i+1]!=NULL) curr_start = i+1; 
			
		}
		//Adding command to temp array 
		else { temp[i - curr_start] = tokens[i]; }
	}

	free(temp); 
	return 1; 
}



int main(int argc, char* argv[]) {
	char  line[MAX_INPUT_SIZE];            
	char  **tokens;              
	int i;

	//Fill the running and curr pid arrays with -1 to mark empty spaces
	for(i=0;i<MAX_NUM_PROCESSES;i++){
			run_pid[i] = -1; 
		}

	for(i=0;i<MAX_NUM_PROCESSES;i++){
			curr_pid[i] = -1; 
		}

	//Checking for existence of file (if passed in as arguement for batch mode)
	FILE* fp;
	if(argc == 2) {
		fp = fopen(argv[1],"r");
		if(fp == NULL) {
			printf("File doesn't exist.");
			return -1;
		}
	}

	//Handle Ctrl+C SIGINT to not terminate terminal, just processes running 
	signal(SIGINT, proccessTerm);

	//BEGIN EXECUTION OF THE SHELL PROMPT LOOP
	while(1) {	

		/* BEGIN: TAKING INPUT */
		bzero(line, sizeof(line));

		// Batch mode, read commands from file
		if(argc == 2) { 
			if(fgets(line, sizeof(line), fp) == NULL) { // file reading finished
				break;	
			}
			line[strlen(line) - 1] = '\0';
		} 


		//Interactive mode, take in user input
		else {
			printf("$ ");
			scanf("%[^\n]", line);
			getchar();
		}
		/* END: TAKING INPUT */
		stop = 0; 
		parallel = 0; 

		checkRunningProc(); 

		//Always clear running processes array
		for(i=0;i<MAX_NUM_PROCESSES;i++){
			curr_pid[i] = -1; 
		}

		//If enter is hit, don't wanna segmentation fault!
		if (strlen(line) == 0) continue; 

		/*Using tokenize function to get array of tokens from input 
		  PROCESS INPUT*/
		line[strlen(line)] = '\n'; //terminate with new line
		tokens = tokenize(line);

		//Variables to check whether to run in foreground and to run regular vs parallel or sequence 
		int regular = 1; 
		int foreground = 1; 

		//Check for &s (BACKGROUND &, SEQUENCE &&, and PARALLEL &&&)
		for(i=0;tokens[i]!=NULL;i++){
			if(strcmp(tokens[i], "&&&") == 0){
				pll_exec(tokens); 
				regular = 0; 
				break; 
			}
			else if(strcmp(tokens[i], "&&") == 0){
				seq_exec(tokens); 
				regular = 0; 
				break; 
			}
			else if((tokens[i+1] == NULL) && (strcmp(tokens[i], "&") == 0)) {
				//Edge case for just & or command starting with & 
				if (strcmp(tokens[0], "&") == 0) {
					printf("Shell: Incorrect Command\n");
					regular = 0; 
					break; 
				}

				//Set to background run mode 
				foreground = 0; 
				//Remove the & from the end of tokens to run correct command
				tokens[i] = '\0';
				break; 
			}
		}

		//Else run normally (No && or &&&, fg or bg)
		if (regular) startCommand(tokens, foreground); 


		// Freeing the allocated memory	
		for(i=0;tokens[i]!=NULL;i++){
			free(tokens[i]);
		}
		free(tokens);

	}
	return 0;
}
