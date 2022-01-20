/*
   Author:       Aaron Nesbit

   Description:  This program is an implementation of a shell called smallsh. It provides a prompt for running
   				 commands, handles blank lines and comments, provides expansion for the variable $$, executes
   				 the commands exit, cd and status, executes other commands by creating new process using exec
   				 functions, supports input and output redirection, supports running commands in foreground
   				 and background processes, and implements customs handlers for 2 signals, SIGINT and SIGSTP.
*/

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// Flag for SIGTSTP initialized
int foreground_mode_flag = 0;

/*
    Custom handler for SIGINT defined
*/
void handle_SIGINT(int signo) {
    char* message = "Caught SIGINT\n";
    write(STDOUT_FILENO, message, 15);
}

/*
    Custom handler for SIGTSTP defined
*/
void handle_SIGTSTP(int signal) {
    if (foreground_mode_flag == 0) {
        // Foreground-only mode is entered and the user is notified
        char* message = "\nEntering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 51);
        foreground_mode_flag = 1;
    }
    else {
        // Foreground-only mode has been exited and the user is notified
        char* message = "\nExiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 31);
        foreground_mode_flag = 0;
    }
}

/*
    Function that replaces occurrences of $$ in a provided string
*/
char* replace_double_dollarsigns(char* str, int pid) {
    char buffer[100], *replacement;
    sprintf(buffer, "%d", pid);
    int i;
    int counter = 0;
    int buffer_length = strlen(buffer);
    for (i = 0; str[i] != '\0'; i++) {
        if (strstr(&str[i], "$$") == &str[i]) {
            i++;
            counter++;
        }
    }
    replacement = (char*)malloc(i + counter * (buffer_length - 2) + 1);
    i = 0;
    while (*str) {
        if (strstr(str, "$$") == str) {
            strcpy(&replacement[i], buffer);
            i += buffer_length;
            str += 2;
        }
        else {
            replacement[i++] = *str++;
        }
    }
    // Converted string is null-terminated
    replacement[i] = '\0';
    // String with $$'s is returned 
    return replacement;
}

/*
* This is main function that runs the shell
*/
int main(void)
{
    // Variables and structs are initialized
    char user_input[2048];
    char* command_line[2048];
    char str[512];
    int child_exit_status = -5;
    int background_mode_flag = 0;
    pid_t spawnPid = -5;
    struct sigaction SIGINT_action = { {0} };
    struct sigaction SIGTSTP_action = { {0} };

    // Signal handler for SIGINT established
    SIGINT_action.sa_handler = SIG_IGN;
    SIGINT_action.sa_flags = 0;
    sigfillset(&(SIGINT_action.sa_mask));
    sigaction(SIGINT, &SIGINT_action, NULL);

    // Signal handler for SIGTSTP established
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    SIGTSTP_action.sa_flags = 0;
    sigfillset(&(SIGTSTP_action.sa_mask));
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // The shell then starts and runs until the 'exit' command is given and exit(0) is executed
    while (1) {
        // Variables are initialized
        char input_file[100] = { 0 };
        char output_file[100] = { 0 };
        int index = 0;
        
        // The command prompt is displayed
        printf(": ");
        fflush(stdout);
        strcpy(user_input, "\n");

        // User's command is collected and stored for parsing
        fgets(user_input, 2048, stdin);

        // Parse the user-entered command
        char* token = strtok(user_input, " ");
        while (token != NULL) {
            // Check for special symbol < for file input
            if (strcmp(token, "<") == 0) {
                token = strtok(NULL, " ");
                sscanf(token, "%s", input_file);
                token = strtok(NULL, " ");
            }
            // Check for special symbol > for file output
            else if (strcmp(token, ">") == 0) {
                token = strtok(NULL, " ");
                sscanf(token, "%s", output_file);
                token = strtok(NULL, " ");
            }
            // Check for $$ so it can be replaced
            else if (strstr(token, "$$") != NULL) {
                sscanf(token, "%s", str);
                // $$ replacement function called
                command_line[index++] = strdup(replace_double_dollarsigns(str, getpid()));
                token = strtok(NULL, " ");
            }
            // Any other tokens go into the command_line array
            else {
                sscanf(token, "%s", str);
                command_line[index++] = strdup(str);
                token = strtok(NULL, " ");
            }
        }

        // If user enters nothing, command_line is set to NULL
        if (user_input[0] == '\n') {
            command_line[0] = NULL;
        }

        // If user entered something and it's not a comment (#)
        if ((command_line[0] != NULL) && (command_line[0][0] != '#')) {
            // See if an & is present indicating a background process
            if (strcmp(command_line[index - 1], "&") == 0) {
                command_line[index - 1] = NULL;
                background_mode_flag = 1;
            }
            else {
                command_line[index++] = NULL;
                background_mode_flag = 0;
            }

            // See if user has entered the 'exit' command
            if (strcmp(command_line[0], "exit") == 0) {
                exit(0);
            }
            // See if user has entered the 'cd' command
            else if (strcmp(command_line[0], "cd") == 0) {
                // If no directory specifed, go to home directory
                if (index - 1 == 1) {
                    chdir(getenv("HOME"));
                }
                // If a directory is specified, go there
                else {
                    chdir(command_line[1]);
                }
            }
            // See if user has entered the 'status' command
            else if (strcmp(command_line[0], "status") == 0) {
                // If the process exited normally the exit status is displayed
                if (WIFEXITED(child_exit_status)) {
                    printf("exit value %i\n", WEXITSTATUS(child_exit_status));
                }
                // If the process was terminated by a signal, termination signal is displayed
                else {
                    printf("terminated by signal %i\n", child_exit_status);
                }
            }
            // All other commands that require a child to be spawned are now handled
            else {
                spawnPid = fork();
                switch (spawnPid) {
                    case -1:
                        // If the fork fails, an error is displayed
                        perror("fork() failed!\n");
                        _exit(1);
                        break;
                    case 0:
                        // The fork is successful and the program continues
                        if (background_mode_flag == 0) {
                            SIGINT_action.sa_handler = SIG_DFL;
                            sigaction(SIGINT, &SIGINT_action, NULL);
                        }
                        if (input_file[0] != 0) {
                            int in = open(input_file, O_RDONLY);
                            if (in == -1) {
                                // If the target file does not exist an error message is displayed
                                printf("%s: no such file or directory\n", input_file);
                                fflush(stdout);
                                _exit(1);
                            }
                            if (dup2(in, 0) == -1) {
                                // If dup2 fails an error message is displayed
                                perror("dup2");
                                _exit(1);
                            }
                            close(in);
                        }
                        if (output_file[0] != 0) {
                            int out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);
                            if (out == -1) {
                                // if the file cannot be opened an error message is displayed
                                printf("cannot open %s\n", output_file);
                                fflush(stdout);
                                _exit(1);
                            }
                            if (dup2(out, 1) == -1) {
                                // If dup2 fails an error message is displayed
                                perror("dup2");
                                _exit(1);
                            }
                            close(out);
                        }
                        if (execvp(command_line[0], command_line) < 0) {
                            // If an invalid command is entered an error message is displayed
                            printf("%s is an invalid command\n", command_line[0]);
                            fflush(stdout);
                            _exit(1);
                        }
                        break;
                    default:
                        // If the process is a foreground process, wait for the process to finish
                        if (foreground_mode_flag == 1 || background_mode_flag == 0) {
                            waitpid(spawnPid, &child_exit_status, 0);
                        }
                        // If the process is a background process, proceed to run in the background
                        else if(background_mode_flag == 1){
                            printf("background pid is %d\n", spawnPid);
                            fflush(stdout);
                        }
                }
            }
            spawnPid = waitpid(-1, &child_exit_status, WNOHANG);
            while (spawnPid > 0) {
                printf("background pid %i is done: ", spawnPid);
                fflush(stdout);
                if (WIFEXITED(child_exit_status)) {
                    printf("exit value %i\n", WEXITSTATUS(child_exit_status));
                    fflush(stdout);
                }
                else {
                    printf("terminated by signal %i\n", child_exit_status);
                    fflush(stdout);
                }
                spawnPid = waitpid(-1, &child_exit_status, WNOHANG);
            }
        }
    }
    // The program ends
	return 0;
}