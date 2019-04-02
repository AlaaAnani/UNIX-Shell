#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <wait.h>
#include <sys/types.h>
#include <fcntl.h> 
#define MAX_LINE 1024               /* The maximum length command */
#define KRED  "\x1B[31m"
#define RESET "\x1b[0m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"

int p_wait_flag = 1;                //parent invokes wait if flag is 1
int input_redirection_flag = 0;     //if 1, default input comes from file
int output_redirection_flag = 0;    //if 1, default output goes to file
int saved_stdin, saved_stdout;      //to restore default input and output settings
int in, infile_index, out, outfile_index; //To handle redirection from and to files
int pipe_flag, pipe_index, p_command2;      
/* ^
pipe_flag   => If command requires communication between two command through a pipe.
pipe_index  => index of "|" in a command.
p_command2  => Starting index of the command after "|" in a pipe command.
*/
int exit_sh = 0;
int empty_command = 0;


void ParseCommand(char * command, char ** args, int command_count, char **command_history_buffer);
void CheckForRedirection( char ** args);
void FlagsDefaultSettings();
void CheckForPipes(char ** args);


int main(void)
{
    printf(YEL"Hello :) \n"RESET);    
    int should_run = 1;     //To determine when to exit the shell program.
    pid_t pid;              //To store process id after forking.
    char **args = malloc(sizeof(char) * (MAX_LINE/2 + 1));          //To store every arguement in a command
    char *command = malloc(sizeof(char) * MAX_LINE);                //To store the currently typed command
    char **command_history_buffer = malloc(sizeof(char) * MAX_LINE); //To store commands and allow for history display
    int command_count = 0;                                          //To count commands
    int state, state2;      //To store state of executing a command in a child and parent process.                          
    int pipefd[2];          //To store piping channels that commands will communicate through
    char ** args1 = malloc(sizeof(char) * (MAX_LINE/2 + 1));    //In case of pipes: args for the command on the left
    char ** args2 = malloc(sizeof(char) * (MAX_LINE/2 + 1));    //In case of pipes: args of the command on the right
    
    
    while(should_run && exit_sh == 0)                                         
    {   

        FlagsDefaultSettings();
        printf(GRN"AAA's>>"RESET);
        memset(command, 0, MAX_LINE);
        fgets (command, MAX_LINE, stdin);
        fflush(stdout);       
        //If user entered exit, return 0. Otherwise, start parsing the command.
        if(strlen(command) <= 0)
        {   
            return 0;      
        } 
        else 
        {
            //Store command in command history buffer
            command_history_buffer[command_count] = malloc(sizeof(char) * strlen(command));
            /*Store it by value since the command will be adjusted in parsing 
            and the buffer needs to store original command.*/
            strcpy(command_history_buffer[command_count], command);
            command_count++;

            ParseCommand(command, args, command_count, command_history_buffer);

            CheckForRedirection(args);
            CheckForPipes(args);

            //Handle redirection if existent
            if(input_redirection_flag == 1 )
            {
                in = open(args[infile_index], O_RDONLY);
                if(in <0)
                {
                    printf("Failed to open file with name %s \n", args[infile_index]);
                    return 1;
                }
                else
                {
                    saved_stdin = dup(0);
                    dup2(in, 0);
                    close(in);
                }                
                
            }

            if(output_redirection_flag == 1)
            {
                out = open("out.txt", O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
                if(in <0)
                {
                    printf("Failed to open file with name %s \n", args[outfile_index]);
                    return 1;
                }
                else
                {
                    saved_stdout = dup(1);
                    dup2(out, 1);
                    close(out);
                }                
                
            }

            //Handle piping if existent
            if(pipe_flag == 1)
            {
                for(int i =0; i <= pipe_index; i++)
                {
                    if(i == pipe_index)
                    {
                        args1[i] = NULL;
                        break;
                    }
                    args1[i] = args[i];
                }
                int last_i = 0;
                for(int i = pipe_index + 1; args[i] != NULL; i++)
                {
                    args2[i - pipe_index - 1] = args[i];  
                    last_i = i;                 
                }
                args2[last_i + 1] = NULL;                
                pipe(pipefd);
                

            }

            //Check for exit
            for(int i=0; args[i] != NULL;i ++)
            {
                if(strcmp(args[i], "exit") == 0)
                exit_sh = 1;
            }

            //Check for empty command
            if(args[0] == NULL || strcmp(args[0], "\0") == 0 || strcmp(args[0], "\n") ==0)
                empty_command = 1;

            if(empty_command == 0)
            {
                if(exit_sh != 1)
                {
                    pid = fork();           //create process

                    if(pid == -1)           //Error occured creating the clone process
                    {
                        printf("PID returned -1. Fork failed.");
                        return 0;
                    }
                    else if(pid == 0)        //The child (new) process
                    {
                        if(input_redirection_flag == 1 | output_redirection_flag == 1)
                        {    
                            
                            char *args_new[] = {args[0], NULL};
                            state = execvp(args[0], args_new);
                        }
                        else if(pipe_flag == 1)
                        {
                            saved_stdout = dup(1);
                            dup2(pipefd[1], 1);     //Replace stdout with the output part of the pipe
                            close(pipefd[0]);       //close unused input side of the pipe
                            execvp(args1[0], args1);//Enter command that will produce output to the other command with ags2
                        }
                        else    //No piping or redirection commands
                        {
                            state = execvp(args[0], args);
                        }
                        
                        if(state <0)
                        printf(KRED"Command %s not found. \n"RESET, args[0]);
                    }
                    else        //Parent process
                    {
                        if(pipe_flag == 1)
                        {
                            saved_stdin = dup(0);
                            dup2(pipefd[0], 0); //Replace stdin with the input part of the pipe
                            close(pipefd[1]);   //close output part of the pipe
                            state2 = execvp(args2[0], args2); //execute command that will take input from the pipe
                            if(state2 <0)
                            printf("Command %s not found. \n", args1[0]);
                        }
                        else if(p_wait_flag == 1)
                        { 
                            wait(NULL);
                        }

                        
                    }
                }
                else
                {
                    printf(YEL"Goodbye. :)\n"RESET);
                    should_run  = 0;
                    
                }
            }
            
            

        }
        //Restore default in and out 
        dup2(saved_stdout, 1);
        dup2(saved_stdin, 0);


    }  

return 0;
}

void ParseCommand(char * command, char ** args, int command_count, char **command_history_buffer)
{
    int args_count =0;                  //to determine the current number of arguements
    int command_len = strlen(command);  //length of the command    
    int arg_i = -1;                     //starting index of the next arguement
    int prev_command = 0;


    //Need to know which command to parse first. If the command is !!, then the previous command is to be parsed.
    if( strcmp(command, "!!\n") == 0 )
    {
        if(command_count <= 0)
            printf("No commands in history");
        else
        {
            strcpy(command, command_history_buffer[command_count-2]); 
            command_len = strlen(command);
            printf("%s", command);
            //for(int i=0; i< command_count; i++)
            //printf(" command !! to  %s \n", command_history_buffer[i]);
            //Let !! execute the most recent entry in the command history buffer before !!
        }
        
        
    }

    for(int i =0; i < command_len; i++)
    {                     

        switch(command[i])
        {
            case ' ':
            case '\t': 
            if(arg_i != -1)           
            {
                args[args_count] = &command[arg_i];
                args_count++;
            }
            command[i] = '\0';         
            arg_i = -1;   
            break;

            case '\n':
            if(arg_i != -1)           
            {
                args[args_count] = &command[arg_i];
                args_count++;
            }
            command[i] = '\0'; 
            args[args_count+1] = NULL;   
            break;

            default:                     

            if(arg_i == -1)
              arg_i = i;
            if(command[i] == '&')
            {
                p_wait_flag = 0;    //parent won't invoke wait()
                command[i] = '\0'; 
                i++;
                args[args_count+1] = NULL; 
            }
            break;
                           

        }    
    }  
    



}

void CheckForRedirection(char ** args)
{
    for(int i=0; args[i] != NULL; i++)
    {
        //printf(" %d %s \n",i, args[i]);
        if(strcmp(args[i], "<") == 0)
        {
            input_redirection_flag = 1;
            if(args[i+1] != NULL)
                infile_index = i+1;
            else
            {
                printf("Command %s has invalid file name. ", args[0]);
            }
            
        }

        if(strcmp(args[i], ">") ==0)
        {
            output_redirection_flag = 1;
            if(args[i+1] != NULL)
                outfile_index = i+1;
            else
            {
                printf("Command %s has invalid file name. ", args[0]);
            }
        }
    }
    //printf("Redirection I O %d %d \n", input_redirection_flag, output_redirection_flag);   

}
void FlagsDefaultSettings()
{
    //default settings for flags
        input_redirection_flag = 0;
        output_redirection_flag = 0;
        p_wait_flag = 1;
        pipe_flag = 0;
        exit_sh  =0;
        empty_command = 0;
}

void CheckForPipes(char ** args)
{
    for(int i=0; args[i] != NULL; i++)
    {
        if(strcmp(args[i], "|") == 0)
        {
            pipe_flag = 1;
            pipe_index = i;
            if(args[i+1] == NULL)
            {
                printf("Invalid command after |. \n");
                
            }
            
            
        }
    }
}
