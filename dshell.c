#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <readline/readline.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>  

char **get_input(char *);
int cd(char *);
void mimicshelldisplay();
void childProcess(char **);
void printcommand(char **);

int main() 
{
    char **command;
    char *input;
    pid_t child_pid, child1_pid, child2_pid;
    int stat_loc;

    /** Used in order to ignore both Ctrl+C(SIGINT) and
        Ctrl+Z(SIGKILL) in the shell*/ 
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    while (1)
     {
        mimicshelldisplay();
        input = readline("$");
        command = get_input(input);
        
        if (!command[0]) 
        {      
        /* Handle empty commands */
            free(input);
            free(command);
            continue;
        }
        
        //Implements the exit function, type "exit" to execute
        if (strcmp(command[0], "exit") == 0) {
            printf("Goodbye!\n");
            exit(0);
        }        
        
        //Flags which seperate between double and triple ampersands
        int iterator = 0,dflag = 0,tflag = 0;
        while(command[iterator])
        {
            if(strcmp(command[iterator],"&&") == 0)
            {
                dflag = 1;
                break;
            }
            if(strcmp(command[iterator],"&&&") == 0)
            {
                tflag = 1;
                break;
            }            
            iterator++;
        }
        
        if(dflag || tflag)
        {
            //Seperating command to two seperate commands in case of ampersand usage
            char string1[1000] = "";
            char string2[1000] = "";        
            int iter = 0;
            while(iter < iterator)
            {
                strcat(string1,command[iter]);
                strcat(string1," ");
                iter++;
            }
            iter++;
            while(command[iter])
            {
                strcat(string2,command[iter]);
                strcat(string2," ");
                iter++;
            }
            char **command1;
            char **command2;
            command1 = get_input(string1);
            command2 = get_input(string2);
        
            child1_pid = fork();
            child2_pid = fork();
            //Considering parallel execution via "&&&"
            if(tflag){
                if (child1_pid < 0 || child2_pid < 0) {
                    printf("Fork failed");
                    exit(1);
                }
                //Parent node
                if (child1_pid > 0 && child2_pid > 0) {
                    waitpid(child1_pid, &stat_loc, WUNTRACED);
                    waitpid(child2_pid, &stat_loc, WUNTRACED);
                }
                //First child
                if (child1_pid == 0 && child2_pid > 0) {
                    childProcess(command1);
                }
                //Second child
                if (child1_pid > 0 && child2_pid == 0) {
                    childProcess(command2);
                }
                //Third child, a byproduct of forking
                else if(child1_pid == 0 && child2_pid == 0) {
                    continue;
                }
            }
            if(dflag){
                if (child1_pid < 0 || child2_pid < 0) {
                    printf("Fork failed");
                    exit(1);
                }
                //Parent node
                if (child1_pid > 0 && child2_pid > 0) {
                    waitpid(child1_pid, &stat_loc, WUNTRACED);
                    waitpid(child2_pid, &stat_loc, WUNTRACED);
                }
                //First child
                if (child1_pid == 0 && child2_pid > 0) {
                    childProcess(command1);
                }
                //Second child
                if (child1_pid > 0 && child2_pid == 0) {
                    waitpid(child1_pid, &stat_loc, WUNTRACED);
                    childProcess(command2);
                }
                //Third child, a byproduct of forking
                else if(child1_pid == 0 && child2_pid == 0) {
                    continue;
                }
            }            
        }
        else
        {
            child_pid = fork();
            if (child_pid < 0) {
                printf("Fork failed");
                exit(1);
            }
            //First child
            if (child_pid == 0) {
                childProcess(command);
            }
            //Parent node
            else {
                waitpid(child_pid, &stat_loc, WUNTRACED);
            }
            
        }
        
        if (!input)
            free(input);
        if (!command)
            free(command);
    }

    return 0;
}

// Returns an array of strings seperated by ' '
char **get_input(char *input) {
    char **command = malloc(8 * sizeof(char *));
    char *separator = " ";
    char *parsed;
    int index = 0;

    // Splits input according to given delimiters.
    parsed = strtok(input, separator);
    while (parsed != NULL) {
        command[index] = parsed;
        index++;

        parsed = strtok(NULL, separator);
    }

    command[index] = NULL;
    return command;
}

//Function to execute cd command
int cd(char *path) {
    return chdir(path);
}

//Function that mimics the way regular shell prompt data is displayed
void mimicshelldisplay()
{
    char cwd[PATH_MAX];
    char hostname[HOST_NAME_MAX];
    char username[LOGIN_NAME_MAX];
    
    //Used to get current directory
    getcwd(cwd, sizeof(cwd));
    //Used for getting login and hostnames, source is unistd.h
    gethostname(hostname, HOST_NAME_MAX);
    getlogin_r(username, LOGIN_NAME_MAX);
    //Mimics output from original shell
    printf("%s@%s:%s",username,hostname,cwd);
}

//Used to execute child process
void childProcess(char **command)
{
    /*Reverts signals back to original behaviors, so that processes
    can be killed during execution in children*/ 
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);

    //Code for implementing alterations to STDIN and STDOUT
    int i = 0;
    while(command[i])
    {
        /*NOTE : alterations for STDOUT are placed here before STDIN 
        as the former has a lower fd index, hence conflicts in allocation
        will not occur. (Else corruption and unexpected behaviour may happen)*/ 
        int altered = 0;
        if(strcmp(command[i],">") == 0) {
            altered = 1;
            close(STDOUT_FILENO);
            open(command[i+1], O_CREAT | O_WRONLY | O_APPEND , S_IRWXU);
        }
        if(strcmp(command[i],"<") == 0) {
            altered = 1;
            close(STDIN_FILENO);
            open(command[i+1], O_RDONLY);                    
        }
        if(altered)
        {
            /* Used in order to remove overhead arguments from the 
            i/o site when stream changing is completed*/
            int j = i;
            while(command[j+2])
            {
                strcpy(command[j],command[j+2]);
                j++;
            }
            command[j] = NULL;
            command[j+1] = NULL;
        }   
        i++;
    }
   
   /** Used in order to detect and execute cd, as it is not a shell
   program but a system call */
   if (strcmp(command[0], "cd") == 0) {
       //Error handling
       if (cd(command[1]) < 0) {
            printf("Error in changing directory. Kindly enter a valid input.");
       }
       /* Skips the fork, as child process is no longer required */
       return;
    }
    
    if (strcmp(command[0], "clear") == 0) {
        printf("\033[H\033[J");
       /* Skips the fork, as child process is no longer required */
       return;
    }
                
    /* Never returns if the call is successful */
    int cmdflag = execvp(command[0],command);
    if (cmdflag < 0) {
        printf("Shell : Incorrect command\n");
        exit(1);
    }

}

//Used for testing purposes 
void printcommand(char **command)
{
    int z = 0;
    while(command[z])
    {
        printf("%s ",command[z]);
        z++;
    }
    printf("%d : length of string",z);
    printf("\n");
}
