#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

#define INPUT_LENGTH 2048
#define MAX_ARGS 512

int *pid_array[MAX_ARGS];

struct command_line
{
    char *argv[MAX_ARGS + 1];
    int argc;
    char *input_file;
    char *output_file;
    bool is_bg;
};

void exit_command()
{
    printf("exit command called\n");
    if (sizeof(pid_array) / sizeof(pid_array[0]))
    {
        exit(1);
    }
}

void cd_command(struct command_line *curr_command)
{
    printf("cd command called\n");

    if (curr_command->argc > 1)
    {
        if (chdir(curr_command->argv[1]) == -1)
        {
            printf("%s: no such file or directory\n", curr_command->argv[1]);
        }
    }
    else
    {
        char *home = getenv("HOME");
        if (home)
        {
            chdir(home);
            setenv("PWD", home, 1);
        }
    }
}

void status_command()
{
    printf("status command called\n");
}

void other_command(struct command_line *curr_command)
{
}

void execute_command(struct command_line *curr_command)
{
  char *newargv[] = { "/bin/ls", "-al", NULL };
  int childStatus;

  pid_t spawnPid = fork();

  switch(spawnPid){
  case -1:
    perror("fork()\n");
    exit(1);
    break;
  case 0:
    printf("CHILD(%d) running ls command\n", getpid());
    execv(newargv[0], newargv);
    perror("execve");
    exit(2);
    break;
  default:
    spawnPid = waitpid(spawnPid, &childStatus, 0);
    printf("PARENT(%d): child(%d) terminated. Now parent is exiting\n", getpid(), spawnPid);
    exit(0);
    break;
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
            curr_command->is_bg = true;
        }
        else
        {
            curr_command->argv[curr_command->argc++] = strdup(token);
        }
        token = strtok(NULL, " \n");
    }
    return curr_command;
}

int main()
{
    struct command_line *curr_command;
    while (true)
    {
        curr_command = parse_input();
        execute_command(curr_command);
    }
    return EXIT_SUCCESS;
}