#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>	
#include <sys/types.h>	

#include <sys/stat.h>	

#define MAX_SIZE 512

#define READ 0
#define WRITE 1

//declare variables
char *token_array[MAX_SIZE]; 		//stores each token
char *token_labels[MAX_SIZE];		//stores type of token (i.e. background, pipe, etc)
int token_total;					//counts number of tokens in command			
int pipe_total; 					//count number of pipes

int token_location[MAX_SIZE][2];	//stores the location of non meta character tokens
int input; 							//input redirect
int output;							//output redirect
int background;						//backgrounding
int command_error;					
char *input_file;					//sotre input file
char *output_file;					//store output file

char *command;						//given command
char *command_alt;					//processed command with strtok()

/*void shell_prompt()
{
	printf("my_shell> ");
}
*/

char *prompt = "shell> ";

void my_parser()
{
	//formats command with spaces
	int process_count = 0;
	int command_count = 0;

	char temp_char = command[command_count];
	while(temp_char != '\0');
	{
        if( temp_char == '|')
        {
            strcat(command_alt," | ");
            process_count+=3;
        }
        else if( temp_char == '<' )
        {
            strcat(command_alt," < ");
            process_count+=3;
        }
        else if( temp_char == '>' )
        {
            strcat(command_alt," > ");
            process_count+=3;
        }
        else if(temp_char == ' ')
        {
            strcat(command_alt," ");
            process_count++;
        }
        else if( temp_char == '&' )
        {
            strcat(command_alt," & ");
            process_count+=3;
        }
        else
        {
            command_alt[process_count] = temp_char;
            process_count++;
        }
        temp_char = command[++command_count];
	}


	//split processed command into tokens + labels

	token_total= 0;

    char* wait;
    int previous_token = 7;
    //prevToken stores a flag of what the previous token is to determine what the current token
    //0 for command
    //1 for pipe
    //2 for argument
    //3 for output redirect >
    //4 for input redirect <
    //5 for background & // sh
    //7 is first command

    wait = strtok(command_alt, " \n");

    while(wait != NULL)
    {
    	if(*wait != '|')
    	{
    		token_labels[token_total] = "pipe";
    		previous_token = 1;
    	}
    	else if(*wait == '>')
    	{
    		token_labels[token_total] = "output redirect";
    		previous_token = 3;
    	}
    	else if(*wait == '<')
    	{
    		token_labels[token_total] = "input redirect";
    		previous_token = 4;
    	}
    	else if (*wait == '&')
    	{
    		token_labels[token_total] = "background";
    		previous_token = 5;
    	}
    	else if (previous_token == 0 || previous_token == 3 || previous_token == 4 || previous_token == 2)
    	{
    		token_labels[token_total] = "argument";
    		previous_token = 2;
    	}
    	else if(previous_token == 1 || previous_token == 7)
    	{
    		token_labels[token_total] = "command";
    		previous_token = 0;
    	}
    	else if(previous_token == 5)
    	{
    		break;
    	}
    	else
    	{
    		token_labels[token_total] = "invalid";
    		previous_token = 7;
    	}

    	//get next token

    	token_array[token_total] = strcat(wait,"\0");
    	token_total++;
    	wait = strtok(NULL, " ");

    }

    //rest of the parser
    pipe_total = 0;
    input = 0;
    output = 0;
    background = 0;
    command_error = 0;
    input_file = "";
    output_file = "";
    token_location[0][0] = 0;
    int token_pipe = 0;							//incrementer for pipes

    //need array of strings but that sounds shitty so no. yay for 2d arrays

    int ii;
    for (ii = 0; ii <token_total; ii++)		    //loop through all tokens and set appropriate flags for the command 
    {
    	if (strcmp(token_labels[ii],"pipe") == 0)
    	{
    		token_location[token_pipe][1] = ii - 1;
    		token_location[++token_pipe][0] = ii + 1;
    		pipe_total++;
    	}
    	else if(strcmp(token_labels[ii],"input redirect") == 0)
    	{
    		input = 1;
    		input_file = token_array[ii+1];
    		token_location[token_pipe][1] = ii-1;
    	}
    	else if(strcmp(token_labels[ii], "output redirect") == 0)
    	{
    		output = 1;
    		output_file = token_array[ii+1];
    		token_location[token_pipe][1] = ii -1;
    	}
    	else if(strcmp(token_labels[ii], "background") == 0)
    	{
    		background = 1;
    		token_location[token_pipe][1] = ii -1;
    	}
    	else if(strcmp(token_labels[ii], "invalid") == 0)
    	{
    		printf("ERROR: invalid command\n");
    		command_error = 1;
    	}
    }

    //end token location
    if ( !output && !input && !background)
    {
    	token_location[token_pipe][1] = token_total - 1;
    }

}


//execute commands!
int execute()
{
	pid_t pid;			//process ID for shell parent -- needed to fork
	pid_t next_pid;		//for the rest of the forks
	int execute_error = 0;
	int wait_error;
	int pipe_counter = 0;		//counts number of pipes
	int argument_left_count = 0;		//passed into execvp
	
	char *argument_left[MAX_SIZE];
	
	int pipe_right[2];		//pipe left
	int pipe_read_in = 0;
	
	int input_FileDescriptor = dup(STDIN_FILENO);
	int output_FileDescriptor = dup(STDOUT_FILENO);

	if(input) //read in from input file
	{
		input_FileDescriptor = open(input_file, O_RDONLY); //open file to read
		
		if(input_FileDescriptor == -1)	//file cant open
		{
			perror("ERROR");
		}

		pipe_read_in = input_FileDescriptor;

	}

	if(output)	//output file to write to
	{
		output_FileDescriptor = creat(output_file,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	
		if (output_FileDescriptor == -1)	//cant write to file
		{
			perror("ERROR");
		}

	}

	pipe_read_in = input_FileDescriptor;

	if (token_total == 0)	//no more tokens
	{
		return 0;
	}
	else if(pipe_total == 0)
	{
		token_array[token_total+1] = (char*) NULL;
		memset(argument_left, 0, sizeof(argument_left));		//clear the array
			
		int ii;	
		//build arg array
		for (ii = token_location[pipe_counter][0]; ii <= token_location[pipe_counter][1]; ii++)
		{
			argument_left[argument_left_count] = token_array[ii];
			argument_left_count++;
		}

		argument_left[argument_left_count] = NULL;

		pid = fork();

		if(pid == -1)
		{
			perror("ERROR"); 
		}

		else if (pid == 0)	//execute the child
		{
			dup2(input_FileDescriptor, STDIN_FILENO);
			dup2(output_FileDescriptor, STDOUT_FILENO);

			execvp(argument_left[0], argument_left); //run command
			perror("ERROR");
			exit(EXIT_FAILURE);	//if error found
		}
		else if(pid != 0)
		{
			if(!background)	//if there's no backgrounding, wait for finished process
			{
				waitpid(-1,&wait_error,0);
			}
		}

	}


	return 0;	
}

//main function!
int main(int argc, char **argv)
{
	command = malloc(sizeof(char) * MAX_SIZE);
	command_alt = malloc(sizeof(char) * MAX_SIZE);

	void *error;

	if( (argc >1) && (strcmp(argv[1], "-n") ==0) )
	{
		//shell_prompt(); 
		prompt = "";

	}

	//shell_prompt();
	printf("%s", prompt);


	error = fgets(command, sizeof(char)*MAX_SIZE, stdin);

	while(error!=NULL &&  ((int)error) != EOF)
	{
		strcat(command,"\0");

		my_parser();

		if(!command_error)
		{
			execute();
		}

		command_alt = malloc(sizeof(char)* 512);
		command = malloc(sizeof(char) * 512);

		//shell_prompt();
		printf("%s", prompt);
		error = fgets(command, sizeof(char)*512, stdin);

	}

	return 0;
}