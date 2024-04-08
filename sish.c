#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_COMMAND_LENGTH 100
#define MAX_PIPES 50
#define MAX_HISTORY 100
#define MAX_ARGS 20

// Parse function for tokenizing the command into an array of arguments
void parse(char *command, char *args[MAX_ARGS]) {
  int i = 0;

  args[0] = strtok(command, " ");
  while (args[i] != NULL) {
    args[++i] = strtok(NULL, " ");
  }
}

// Prints full history along with offset indecies
void printHistory(char history[MAX_HISTORY][MAX_COMMAND_LENGTH], int *front,
                  int *rear) {
  int32_t i = *front;
  int32_t counter = 0;
  while (i != *rear) {
    printf("%d %s\n", counter, history[i]);
    counter = (counter + 1) % MAX_HISTORY;
    i = (i + 1) % MAX_HISTORY;
  }
  printf("%d %s\n", counter, history[i]);
}

// Function for adding the most recent command to history
void addToHistory(char history[MAX_HISTORY][MAX_COMMAND_LENGTH],
                  char command[MAX_COMMAND_LENGTH], int *front, int *rear) {
  if (*rear == MAX_HISTORY - 1) {
    *front = (*front + 1) % MAX_HISTORY;
  }
  if (*front == -1) {
    *front = 0;
  } else if (*front - 1 == *rear) {
    *front = (*front + 1) % MAX_HISTORY;
  }

  *rear = (*rear + 1) % MAX_HISTORY;

  strncpy(history[*rear], command, (MAX_COMMAND_LENGTH));
}

// cd built-in command handling
void cdhandler(char *args[MAX_ARGS]) {
  char user[100];
  char root[120];
  strcpy(root, "/Users/");
  if (getlogin_r(user, sizeof(user)) != 0) {
    perror("error getting user");
    return;
  }
  strcat(root, user);
  if (args[1] == NULL) {
    if (chdir(root) != 0) {
      perror("error changing to root");
    }
  } else {
    if (chdir(args[1]) != 0) {
      perror("chdir error");
    }
  }
}

// Function to handle non-piped commands
void execute(char *command, char *args[MAX_ARGS], int *front, int *rear,
             char history[MAX_HISTORY][MAX_COMMAND_LENGTH]) {
  if (strcmp(command, "exit") == 0) {
    exit(0); // Exit the shell if "exit" is entered
  } else if (strcmp(args[0], "cd") == 0) {
    cdhandler(args); // Change to the directory provided in args[]
  } else if (strcmp(args[0], "history") == 0) {
    // History handling, clearing, and executing based on argument
    if (args[1] != NULL && strcmp(args[1], "-c") == 0) {
      *front = -1;
      *rear = -1;
    } else if (args[1] != NULL) {
      int32_t commandIndex;
      if (*front > *rear) {
        commandIndex = ((*front + (*args[1] - 48)) - 1) % MAX_HISTORY;
      } else {
        commandIndex = ((*front + (*args[1] - 48))) % MAX_HISTORY;
      }
      if (commandIndex < 100 && commandIndex >= 0) {
        parse(history[commandIndex], args);
        execute(command, args, front, rear, history);
      } else {
        printf("%s", "Unknown argument for history");
      }
    } else {
      printHistory(history, front, rear);
    }
  } else {
    pid_t pid = fork();

    if (pid == 0) {
      if (execvp(args[0], args) == -1) {
        perror("execvp failed");
        exit(EXIT_FAILURE);
      }
    } else if (pid > 0) {
      int status;
      waitpid(pid, &status, 0);
    } else {
      perror("fork error\n");
    }
  }
}

// Function to handle piped commands
void executePipe(int32_t countPipes, char *pipedCommands[MAX_PIPES]) {
  int fds[countPipes + 1][2];

  int pipeI = 0;
  for (pipeI = 0; pipeI <= countPipes; pipeI++) {
    if (pipe(fds[pipeI]) < 0) {
      perror("pipe error\n");
      return;
    }
  }

  pid_t pids[countPipes + 1];

  for (int i = 0; i <= countPipes; i++) {

    pid_t pid = fork();

    if (pid == 0) {
      if (i > 0) {
        dup2(fds[i - 1][0], STDIN_FILENO);
        close(fds[i - 1][0]);
      }
      if (i < countPipes) {
        dup2(fds[i][1], STDOUT_FILENO);
        close(fds[i][1]);
      }

      for (int j = 0; j <= countPipes; j++) {
        close(fds[j][0]);
        close(fds[j][1]);
      }

      char *args[MAX_ARGS];
      parse(pipedCommands[i], args);
      execvp(args[0], args);
      perror("execvp failed in pipe command\n");
      exit(1);
    } else {
      pids[i] = pid;
    }
  }

  for (int i = 0; i <= countPipes; i++) {
    close(fds[i][0]);
    if (i != countPipes) {
      close(fds[i][1]);
    }
  }

  for (int i = 0; i <= countPipes; i++) {
    int status;
    waitpid(pids[i], &status, 0);
  }
}

int32_t piped(char *command, char *pipedCommands[MAX_PIPES]) {
  int32_t i = 0, countPipes = -1;

  pipedCommands[0] = strsep(&command, "|");
  while (pipedCommands[i] != NULL) {
    pipedCommands[++i] = strsep(&command, "|");
    countPipes++;
  }
  return countPipes;
}

int main(int argc, char **argv) {
  size_t maxCommandLength = 100;
  int32_t front = -1, rear = -1;
  int32_t flag = 1;

  char history[MAX_HISTORY][MAX_COMMAND_LENGTH];
  while (flag) {
    char *command = NULL;
    char *pipedCommands[MAX_PIPES];
    char cwd[PATH_MAX];
    char user[100];
    // Prompt, line reading, and adds the command to history
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      printf("%s> ", cwd);
    } else {
      perror("cwd error");
      return 1;
    }
    // printf();
    if (getline(&command, &maxCommandLength, stdin) <= 0 ||
        strcmp(command, "\n") == 0) {
      continue;
    }

    command[strcspn(command, "\n")] = '\0';
    addToHistory(history, command, &front, &rear);

    // Execute based on if pipes are present
    int32_t countPipes = piped(command, pipedCommands);
    if (countPipes > 0) {
      executePipe(countPipes, pipedCommands);
    } else {
      char *args[MAX_ARGS];
      parse(command, args);
      execute(command, args, &front, &rear, history);
    }
  }
}
