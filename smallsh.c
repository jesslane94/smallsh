#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

//fg process flag: if 1, the process is in the foreground
int foreground;
//array to hold PIDs of bg processes
int pidArray[100] = {0};
//count of bg processes
int count = 0;
//indicator for if user enters a comment
int isComment = 0;
//flag for in fg only mode or not
int fgOnlyMode = 0;


/*
* void parseLine
* code from csl.mtu.edu/cs4411.ck/www/NOTES/process/fork/exec.html
* this function sets the arguments to pass to exec. 
*/
void parseLine(char* line, char **argv)
{
	while(*line != '\0') 
	{
		//replace whitespaces/tabs/endline with terminating null character
		while(*line == ' ' || *line == '\t' || *line == '\n')
		{
			*line++ = '\0';
		}
		//add to argument array
		*argv++ = line;
		//iterate along line
		while (*line != '\0' && *line != ' ' && *line != '\t' && *line != '\n')
		{
			line++;
		}
	}
	//get rid of trailing character/set last argument to null
	*argv--;
	*argv = NULL;
}

/*
* char *cleanLine
* this article helped me: stackoverflow.com/questions/14042047/copy-specific-characters-from-a-string-to-another-string
* replaces $$ with PID, checks for comment
*/
char *cleanLine(char *input)
{
	int i;
	//pointer to where we find the $$
	char *p;
	//will hold the stringified PID
	char holdPID[15];
	//grab actual pid
	int shellID = getpid();
	char tempHold[256] = "";
	
	//while we are here, check if it is a comment
	if(strncmp(&input[0], "#", 1) == 0)
	{
		isComment = 1;
	}

	//if $$ is not found, go ahead and return the input
	if(strstr(input, "$$") == NULL)
	{
		return input;
	}	

	//if $$ is found, continue to parsing
	while(strstr(input, "$$") != NULL)
	{
		i=0;
		//pointer to where the $$ is
		p = strstr(input, "$$");		
		//change PID to string		
		sprintf(holdPID, "%d", shellID);
		//concatenate chars together as long as they arent the $$ characters
		while(&input[i] != p)
		{
			strncat(tempHold, &input[i], 1);
			i++;
		}
		//pass the $$ characters
		i = i+2;
		//catenate the characters we have so far with the stringified PID
		strcat(tempHold, holdPID);
		
		//concatenate the string that's after the $$ we just encountered
		while(i<strlen(input))
		{
			strncat(tempHold, &input[i], 1);
			i++;
		}
		//tack on terminator
		strcat(tempHold, "\0");
		//set input = to our new string, to continue to search down the line for $$
		input = tempHold;
	}
	//return our new string without the $$'s
	return tempHold;
}

//from Signals lecture
void catchSIGINT(int signo)
{
	//show message if sent SIGINT
	char* message = "terminated by signal 2.\n";
	write(STDOUT_FILENO, message, 24);
}

void catchSIGTSTP(int signo)
{
	//toggle fg process, if 0 set to 1 to indicated that we are in fg only mode
	if(fgOnlyMode == 0)
	{
		fgOnlyMode = 1;
		char *message = "\nentering foreground-only mode (& is now ignored)\n";
		write(STDOUT_FILENO, message, 50);		
	}
	else
	{
		//switch off foreground only mode
		fgOnlyMode = 0;
		char *message = "\nexiting foreground-only mode.\n";
		write(STDOUT_FILENO, message, 31);	
	}
}

void printStatus(int status)
{
	//from 3.1 Processes slides
	if (WIFEXITED(status))
	{
		int actualExit = WEXITSTATUS(status);
		printf("exit value %d\n", actualExit);
	}
	else
	{
		int actualSignal = WTERMSIG(status);
		printf("terminated by signal %d.\n", actualSignal);
	}
}


int main()
{	
	//initialize variables to hold user input
	size_t bufferSize = 0;
	char* userLine = NULL;
	//tempLine is to hold line after $$ have been replaced
	char* tempLine = NULL;
	int numCharsEntered = -5;
	
	//char array to hold arguments inputted by user after they have been parsed out
	char *arguments[64];

	//initialize variables to hold spawned process information
	pid_t childPid;
	int childExitStat;	

	//signal catching stuff modeled after the lecture
	//initialize structs 
	struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};
	//set default to ignore, don't want shell to terminate
	SIGINT_action.sa_handler = SIG_IGN;
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_action, NULL);

	//set up sigtstp actions
	SIGTSTP_action.sa_handler = catchSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = 0;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	while(1)
	{
		//set as foreground process to begin with
		foreground = 1;

		//initialize variables for file I/O, names and file descriptors 
		char inputFileName[256] = "";
		char outputFileName[256] = "";
		int sourceFD, targetFD, result;
	
		while(1)
		{
			//make sure colon gets printed
			printf(": ");
			fflush(stdout);
			//use getline and hold num chars entered for error checking
			numCharsEntered = getline(&userLine, &bufferSize, stdin);
			if (numCharsEntered == -1) 
			{ 
				clearerr(stdin); 
			}
			else
			{
				break;
			}
		}
	
		//if they entered in commands/text
		if(numCharsEntered != 1)
		{
			//parse out command name from user input to pass to execvp (store in an array?)
			tempLine = cleanLine(userLine);
			//copy new line back into the userLine variable
			strcpy(userLine, tempLine);
			
			//make sure comments aren't read by setting argument for later loop
			if(isComment == 1)
			{
				arguments[0] = "#";
				arguments[1] = NULL;
				isComment = 0;
			}
			else
			{
				//assign arguments
				parseLine(userLine, arguments);
			}

			int i = 0;
			//check for file input/outputs
			while(arguments[i] != NULL)
			{
				//found input redirection
				if(strcmp(arguments[i], "<") == 0)
				{
					//get rid of the symbol
					arguments[i]= NULL;
					//copy the file name into the char array 
					strcpy(inputFileName, arguments[i+=1]);
					//set where the file name was to NULL
					arguments[i] = NULL;		
				}
				//found output redirection
				else if(strcmp(arguments[i], ">") == 0)
				{
					//get rid of symbol
					arguments[i] = NULL;
					//copy file name into char array
					strcpy(outputFileName, arguments[i+=1]);
					//set where filename was to NULL
					arguments[i] = NULL;
				}
				//indicate background process
				else if(strcmp(arguments[i], "&") == 0)
				{
					//make sure we aren't in fg only mode, and avoid parsing echo statements
					if(fgOnlyMode == 0 && strcmp(arguments[0], "echo") != 0)
					{
						foreground = 0;
						arguments[i] = NULL;
					}
					//otherwise, just get rid of the & for proper exec
					else
					{
						arguments[i] = NULL;
					}
				}
				i++;
			}
		}
		else
		{
			//if they only pressed enter, set the first argument to empty string
			arguments[0] = "";	
		}

		//check for built in commands
		if (strcmp(arguments[0], "exit") == 0)
		{
			exit(0);
		}
		//continue on if comment or the user just pressed enter
		else if (strcmp(arguments[0], "#") == 0 || strcmp(arguments[0], "") == 0 || isComment == 1)
		{	
			;
		}
		//print status of previous commands
		else if (strcmp(arguments[0], "status") == 0)
		{
			printStatus(childExitStat);
		}
		//we are moving around directories
		else if (strcmp(arguments[0], "cd") == 0)
		{
			//if they type in just cd, go to home
			if(arguments[1] == NULL)
			{
				chdir(getenv("HOME"));
			}
			//otherwise, go to where the user typed
			else
			{
				if(chdir(arguments[1]) != 0)
				{
					printf("%s is not a valid directory.", arguments[1]);
				}
			}
		}
		else
		{
			//fork child process: switch statements taken from notes
			childPid = fork();
			switch(childPid)
			{
				//error if fork fails
				case -1: { perror("Error executing fork!\n"); exit(1); break; }
				//in child
				case 0: 
				{
					//if in foreground process, it can be interrupted without killing the shell
					if(foreground == 1)
					{
						SIGINT_action.sa_handler = SIG_DFL;
						sigaction(SIGINT, &SIGINT_action, NULL);
					}
					
					//if there are file input/outputs create the files to be read from/written to
					//from 3.4 More Unix IO slides					
					if(strcmp(inputFileName, "") != 0)
					{
						//open the file for reading only
						sourceFD = open(inputFileName, O_RDONLY);
						if(sourceFD == -1)
						{
							fprintf(stderr, "cannot open %s for input\n", inputFileName);
							exit(1);
						}
						result = dup2(sourceFD, 0);
						if (result == -1) { perror("source dup2()"); exit(2); }

						//close file on exec
						fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
					}
					//if the process is in the background and has no file redirection, direct to the null file
					else if(foreground == 0 && strcmp(inputFileName, "") == 0)
 					{
						sourceFD = open("/dev/null", O_RDONLY);
						result = dup2(sourceFD, 0);
					} 
					//set up output redirection if there is a file
					if(strcmp(outputFileName, "") != 0)
					{
						//open file or create new one to write to if it doesn't exist
						targetFD = open(outputFileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
						if(targetFD == -1)
						{
							fprintf(stderr, "cannot open %s for output\n", outputFileName);
							exit(1);
						}
						result = dup2(targetFD, 1);
						if (result == -1) { perror("target dup2()"); exit(2); }
						
						//close file on exec
						fcntl(targetFD, F_SETFD, FD_CLOEXEC);
					}
					//if its a background process without a output redirect, direct to null file
					else if(foreground == 0 && strcmp(outputFileName, "") == 0)
 					{
						sourceFD = open("/dev/null", O_RDONLY);
						result = dup2(sourceFD, 0);
					} 
					
					//execute arguments once everything has been set up
					if(execvp(arguments[0], arguments) < 0)
					{
						//perror("exec failure");
						printf("%s: no such file or directory\n", arguments[0]);
						fflush(stdout);
						exit(1);
					}
					break;
				}
				//parent stuff
				default: 
				{
					//if we are in the foreground, wait for fg processes to end
					int i;
					pid_t pid;
					if(foreground == 1)
					{
						
						waitpid(childPid, &childExitStat, 0);
						//when process ends, print exit status if there is a signal
						if(WIFSIGNALED(childExitStat) != 0)
						{
							printf("terminated by signal %d\n", WTERMSIG(childExitStat));
							fflush(stdout);
						}
					}
					//we are in bg process
					else
					{
						//print pid of new bg process
						printf("background PID is: %d\n", childPid);
						//add to array
						pidArray[count] = childPid;
						//increase count
						count++;
						break;
					}
					
				}
			}
		
		}

		//little cleanup
		free(userLine);
		userLine = NULL;

		//combination of notes and
		//https://stackoverflow.com/questions/35722491/parent-process-waits-for-all-child-processes-to-finish-before-continuing
		pid_t kidPid;
		int i = 0;
		//for each bg process
		for(i=0; i<count; i++)
		{
			//see if process has completed
			kidPid = waitpid(pidArray[i], &childExitStat, WNOHANG);
			if(kidPid)
    		{
				//when bg process has ended, print status
        		printf("background pid %d is done: ", kidPid);
				printStatus(childExitStat);
				count--;
    		}
		}
	}
	return 0;
}	


