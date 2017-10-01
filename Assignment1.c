
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

struct node {
  int number;
  int pid;
  struct node *next;
};

struct node *head_job = NULL;
struct node *current_job = NULL;
struct node *copy_head_job = NULL;
pid_t current_fg_job_pid = NULL;

//Initiliaze the args(input) to null once the command has been processed
// this is to clear it to accept another command in the next while loop
void initialize(char *args[]);


int getcmd(char *line, char *args[], int *background)
{
	int i = 0;
	char *token, *loc;

	//Copy the line to a new char array because after the tokenization a big part of the line gets deleted since the null pointer is moved
	char *copyCmd = malloc(sizeof(char) * sizeof(line) * strlen(line));
	sprintf(copyCmd, "%s", line);

	// Check if background is specified..
	if ((loc = index(line, '&')) != NULL) {
		*background = 1;
		*loc = ' ';
	} else
		*background = 0;

	//Create a new line pointer to solve the problem of memory leaking created by strsep() and getline() when making line = NULL
	char *lineCopy = line;
	while ((token = strsep(&lineCopy, " \t\n")) != NULL) {
		for (int j = 0; j < strlen(token); j++)
			if (token[j] <= 32)
				token[j] = '\0';
		if (strlen(token) > 0)
			args[i++] = token;
	}

	return i;
}

/* Add a job to the list of jobs
 */
void addToJobList(char *args[], int process_pid) {

	struct node *job = malloc(sizeof(struct node));

	//If the job list is empty, create a new head
	if (head_job == NULL) {
		job->number = 1;
		job->pid = process_pid;

		//the new head is also the current node
		job->next = NULL;
		head_job = job;
		current_job = head_job;
	}

	//Otherwise create a new job node and link the current node to it
	else {

		job->number = current_job->number + 1;
		job->pid = process_pid;

		current_job->next = job;
		current_job = job;
		job->next = NULL;
	}
}


void initialize(char *args[]) {
	for (int i = 0; i < 20; i++) {
		args[i] = NULL;
	}
	return;
}

// This function handles Signals
void sighandler(int signal)
{
    if (signal == SIGTSTP)
        printf("\nReceived (SIGTSTP)\n");
    else if (signal == SIGINT)
        printf("\nreceived (SIGINT)\n");
        printf("Killing foreground job...\n");
        kill(getpid(), SIGKILL);
}

int main(void) {

  ///// SIGNALS
  if (signal(SIGTSTP, sighandler) == SIG_ERR)
        printf("\nCan't catch SIGUSR1\n");
  if (signal(SIGINT, sighandler) == SIG_ERR)
        printf("\nCan't catch SIGINT\n");



  ////////////////////

	char *args[20];
	int bg;

	char *user = getenv("USER");
	if (user == NULL) user = "User";

	char str[sizeof(char)*strlen(user) + 4];
	sprintf(str, "\n%s>> ", user);

	while (1) {
		initialize(args);
		bg = 0;

		int length = 0;
		char *line = NULL;
		size_t linecap = 0; // 16 bit unsigned integer
		sprintf(str, "\n%s>> ", user);
		printf("%s", str);

		/*
		Reads an entire line from stream, storing the address of
		the buffer containing the text into *lineptr.  The buffer is null-
		terminated and includes the newline character, if one was found.
		check the linux manual for more info
		*/


		length = getline(&line, &linecap, stdin);
		if (length <= 0) { //if argument is empty
			exit(-1);
		}
		int cnt  = getcmd(line, args, &bg);

// Command LS
		if (!strcmp("ls", args[0])) { // returns 0 if they are equal , then we negate to make the if statment true


			printf("You're trying to call ls \n");

			pid_t  pid;// this is just a type def for int really..
			pid = fork();
			if (pid == 0) {
				execvp(args[0], args);
      }
			else {
        if (bg == 1) {
            addToJobList(args, pid);
        } else {
          current_fg_job_pid = pid;
        }
				int status;
				waitpid(pid, &status, WUNTRACED);
			}

		}

// Command CD
		else if (!strcmp("cd", args[0])) {

			int result = 0;
			if (args[1] == NULL) { // this will fetch home directory if you just input "cd" with no arguments
				char *home = getenv("HOME");
				if (home != NULL) {
					result = chdir(home);
				}
				else {
					printf("cd: No $HOME variable declared in the environment");
				}
			}
			//Otherwise go to specified directory
			else {
				result = chdir(args[1]);
			}
			if (result == -1) fprintf(stderr, "cd: %s: No such file or directory", args[1]);

		}

// Command CAT
    else if (!strcmp("cat", args[0])) {
      printf("You're trying to call cat \n");

			pid_t  pid;// this is just a type def for int really..
			pid = fork();
			if (pid == 0)
				execvp(args[0], args);
			else {
        if (bg == 1) {
            addToJobList(args, pid);
            // Implement CAT
        } else {
          current_fg_job_pid = pid;
        }
				int status;
				waitpid(pid, &status, WUNTRACED);
			}
    }

    else if (!strcmp("cp", args[0])) {
      printf("You're trying to call cp \n");

			pid_t  pid;// this is just a type def for int really..
			pid = fork();
			if (pid == 0) {
				execvp(args[0], args);
      }
			else {
        if (bg == 1) {
            addToJobList(args, pid);
            // Implement CP
        } else {
          current_fg_job_pid = pid;
        }
				int status;
				waitpid(pid, &status, WUNTRACED);
			}
    }

// Command FOREGROUND
    else if (!strcmp("fg", args[0])) {
      if (head_job == NULL) {
        printf("No job is running in background.");
      } else {
        while (head_job->next != NULL) {
          if (head_job->number == atoi(args[1])) {
            printf("Job number %d, PID:%d is put in foreground.",head_job->number, head_job->pid);
          }
        }
      }
    }

// Command JOBS
    else if (!strcmp("jobs", args[0])) {
      if (head_job == NULL) {
        printf("No job is running in background.");
      } else {
        copy_head_job = head_job;
        while (copy_head_job != NULL) {
          printf("[%d] , PID: %d",head_job->number, head_job->pid);
          copy_head_job = copy_head_job->next;
        }
      }
    }

////// OTHER BUILD IN COMMANDS

// Command PWD
    else if (!strcmp("pwd", args[0])) {
      printf("%s",getcwd(NULL, (int)NULL));
    }

// Command TIMES
    else if (!strcmp("eval", args[0])) {
      printf("%d",execvp(args[0], args));
    }



// Command EXIT
    else if (!strcmp("exit", args[0])) {
		    exit(0);
    }

// ANY OTHER COMMANDS
    else {
      if(execvp(args[0], args) < 0){
        printf("No command found");
      }
    }
	}


}
