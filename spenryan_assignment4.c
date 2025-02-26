#include <stdio.h>      // For printf, fflush
#include <stdlib.h>     // For exit, malloc, free
#include <unistd.h>     // For fork, execvp, getcwd, chdir, sleep
#include <sys/types.h>  // For pid_t
#include <sys/wait.h>   // For waitpid
#include <fcntl.h>      // For open, close, O_RDONLY, O_WRONLY
#include <signal.h>     // For signal handling
#include <stdbool.h>    // For boolean values (true, false)
#include <string.h> 
#define _GNU_SOURCE

#define INPUT_LENGTH 2048
#define MAX_ARGS 512

pid_t pid_array[MAX_ARGS];
int last_exit_status = 0;
bool foreground_only = false;

struct command_line
{
    char *argv[MAX_ARGS + 1];
    int argc;
    char *input_file;
    char *output_file;
    bool is_bg;
};

void handle_SIGINT(int signo)
{
    printf("\nterminated by signal %d\n", signo);
    fflush(stdout);
}

void handle_SIGTSTP(int signo)
{
    if (!foreground_only) {
        foreground_only = true;
        printf("\nEntering foreground-only mode (& is now ignored)\n");
    } else {
        foreground_only = false;
        printf("\nExiting foreground-only mode\n");
    }
    fflush(stdout);
}

void check_background_processes()
{
    int childStatus;
    pid_t bg_pid;
    while ((bg_pid = waitpid(-1, &childStatus, WNOHANG)) > 0) {
        printf("background pid %d is done: ", bg_pid);
        if (WIFEXITED(childStatus)) {
            printf("exit value %d\n", WEXITSTATUS(childStatus));
        } else if (WIFSIGNALED(childStatus)) {
            printf("terminated by signal %d\n", WTERMSIG(childStatus));
        }
    }
}

void exit_command()
{
    // Kill all background processes before exiting
    for (int i = 0; i < MAX_ARGS; i++) {
        if (pid_array[i] > 0) {
            kill(pid_array[i], SIGTERM);
        }
    }
    exit(0);
}


void cd_command(struct command_line *curr_command)
{
    if (curr_command->argc == 1) {
        chdir(getenv("HOME"));
    } else {
        if (chdir(curr_command->argv[1]) != 0) {
            perror("cd failed");
        }
    }
}

void status_command()
{
    if (WIFEXITED(last_exit_status)) {
        printf("exit value %d\n", WEXITSTATUS(last_exit_status));
    } else if (WIFSIGNALED(last_exit_status)) {
        printf("terminated by signal %d\n", WTERMSIG(last_exit_status));
    }
}

void other_command(struct command_line *curr_command)
{
    int childStatus;
    pid_t spawnPid = fork();

    switch (spawnPid) {
        case -1:
            perror("fork() failed");
            exit(1);
            break;
        case 0:
            // CHILD PROCESS
            curr_command->argv[curr_command->argc] = NULL;  // Ensure NULL termination

            // Handle Input Redirection
            if (curr_command->input_file) {
                int input_fd = open(curr_command->input_file, O_RDONLY);
                if (input_fd == -1) {
                    perror("cannot open badfile for input\n");
                    exit(1);
                }
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
            }

            // Handle Output Redirection
            if (curr_command->output_file) {
                int output_fd = open(curr_command->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_fd == -1) {
                    perror("cannot open output file");
                    exit(1);
                }
                dup2(output_fd, STDOUT_FILENO);
                close(output_fd);
            }

            // Execute command
            execvp(curr_command->argv[0], curr_command->argv);
            perror("execvp failed");
            exit(2);
            break;

        default:
            // PARENT PROCESS
            if (!curr_command->is_bg) {
                waitpid(spawnPid, &childStatus, 0);
            } else {
                printf("background pid is %d\n", spawnPid);
            }
            break;
    }
}


void execute_command(struct command_line *curr_command)
{
    if (strcmp(curr_command->argv[0], "exit") == 0)
    {
        exit_command();
    }
    else if (strcmp(curr_command->argv[0], "cd") == 0)
    {
        cd_command(curr_command);
    }
    else if (strcmp(curr_command->argv[0], "status") == 0)
    {
        status_command();
    }
    else
    {
        other_command(curr_command);
    }
}

struct command_line *parse_input()
{
    char input[INPUT_LENGTH];
    struct command_line *curr_command = (struct command_line *)calloc(1, sizeof(struct command_line));

    // Get input
    printf(": ");
    fflush(stdout);
    fgets(input, INPUT_LENGTH, stdin);

    if (!strcmp(input, "#")){
      curr_command = NULL;
      return curr_command;
    }

    // Tokenize the input
    char *token = strtok(input, " \n");
    while (token)
    {
        if (!strcmp(token, "<"))
        {
            curr_command->input_file = strdup(strtok(NULL, " \n"));
        }
        else if (!strcmp(token, ">"))
        {
            curr_command->output_file = strdup(strtok(NULL, " \n"));
        }
        else if (!strcmp(token, "&"))
        {
            if (!foreground_only) {
                curr_command->is_bg = true;
            }
        }
        else
        {
            curr_command->argv[curr_command->argc++] = strdup(token);
        }
        token = strtok(NULL, " \n");
    }
    curr_command->argv[curr_command->argc] = NULL;
    return curr_command;
}

int main()
{
    struct command_line *curr_command;
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};

    // Ignore SIGINT by default (only foreground processes should handle it)
    SIGINT_action.sa_handler = handle_SIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &SIGINT_action, NULL);

    // Handle SIGTSTP to toggle foreground-only mode
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    while (true)
    {
        curr_command = parse_input();
        if (curr_command == NULL) 
        {
            continue; // Ignore empty input
        } 
        else 
        {
          execute_command(curr_command);
          check_background_processes();
          free(curr_command);
        }

    }
    
    return EXIT_SUCCESS;
}
