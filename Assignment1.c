
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

// ==== NODE STRUCTURE
struct node {
  int number;
  int pid;
  struct node *next;
};


// ==== GLOBAL VARIABLES
struct node *head_job = NULL;
struct node *current_job = NULL;
struct node *copy_head_job = NULL;
pid_t current_fg_job_pid;


//
int getcmd(char *line, char *args[], int *background, int *output_redirect)
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
	} else *background = 0;

  // Check if output redirection is specified..
  if (index(line, '>') != NULL) *output_redirect=1;

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


void clean_arguments(char *args[]) {
	for (int i = 0; i < 20; i++) {
		args[i] = NULL;
	}
}

void redirect_output(char *args[]) {
  int array_index = 0;
  char *filename = "";
  while (args[array_index] != '\0') {
    if (strcmp(args[array_index++], ">") == 0) {
      if (args[array_index] != NULL) {
      filename = args[array_index];
      args[array_index] = NULL;
      args[array_index - 1] = NULL;
      int fd = open(filename, O_CREAT | O_WRONLY, 0777 );

      close(1);
      dup(fd);
      close(fd);
      } else {
        perror("filename is not provided");
        exit(1);
      }
    }
  }
}

int myexecvp(char *args[], int *output_redirect) {
  int result = 0;
  if (output_redirect == 0) { result = execvp(args[0], args);}
  else {redirect_output(args); result = execvp(args[0], args);}

  return result;
}

void execute_with_fork(char *args[], int *bg, int *output_redirect, int *current_fg_job_pid){
  pid_t  pid;
  pid = fork();
  if (pid == -1) {
    printf("Forking was unsuccessful..\n");
  }
  else if (pid == 0) {
    myexecvp(args, output_redirect);
  }
  else {
    if (*bg == 1) {
        addToJobList(args, pid);
    } else {
      *current_fg_job_pid = pid;
    }
    int status;
    waitpid(pid, &status, WUNTRACED);
  }
}

// This function handles Signals
void sighandler(int signal)
{
    if (signal == SIGTSTP) {
    // Ignore Ctrl+Z
    }
    else if (signal == SIGINT) {
        kill(current_fg_job_pid, SIGKILL);
      }
}

int main(void) {

  //========= Signals Handling===============
  if (signal(SIGTSTP, sighandler) == SIG_ERR) exit(1);
  if (signal(SIGINT, sighandler) == SIG_ERR) exit(1);

  ////////////////////

	char *args[20];
	int bg, output_redirect;

	while (1) {

    // ===== RESET VARIABLES
		clean_arguments(args);
    copy_head_job = head_job;
		bg = 0;
    output_redirect = 0;
		int length = 0;
		char *line = NULL;
		size_t linecap = 0; // 16 bit unsigned integer
    //=========================

		printf("%s", "$>>");

		/*
		Reads an entire line from stream, storing the address of
		the buffer containing the text into *lineptr.  The buffer is null-
		terminated and includes the newline character, if one was found.
		*/


		length = getline(&line, &linecap, stdin);

    // If argument is empty
		if (length <= 0) exit(-1);

		int cnt  = getcmd(line, args, &bg, &output_redirect);

    if (cnt != 0) {

      // Command LS
  		if (!strcmp("ls", args[0])) {
        execute_with_fork(args, &bg, &output_redirect, &current_fg_job_pid);
  		}
      // Command CAT
      else if (!strcmp("cat", args[0])) {
        execute_with_fork(args, &bg, &output_redirect, &current_fg_job_pid);
      }

      // Command Copy
      else if (!strcmp("cp", args[0])) {
        execute_with_fork(args, &bg, &output_redirect, &current_fg_job_pid);
      }

      // Command Move
      else if (!strcmp("mv", args[0])) {
        execute_with_fork(args, &bg, &output_redirect, &current_fg_job_pid);
      }

      //Command Remove
      else if (!strcmp("rm", args[0])) {
        execute_with_fork(args, &bg, &output_redirect, &current_fg_job_pid);
      }

      // Command Touch for creating files
      else if (!strcmp("touch", args[0])) {
        execute_with_fork(args, &bg, &output_redirect, &current_fg_job_pid);
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


      // Command FOREGROUND
      else if (!strcmp("fg", args[0])) {
        if (head_job == NULL) {
          printf("No job is running in background.\n");
        } else {
          if (args[1] != '\0') {
            while (copy_head_job!= NULL) {
              if (copy_head_job->number == atoi(args[1])) {
                printf("Job number %d, PID:%d is put in foreground.\n",copy_head_job->number, copy_head_job->pid);
                myexecvp(args, &output_redirect);
                break;
              }
              copy_head_job = copy_head_job->next;
            }
          } else {
            printf("%s\n","Please specify job number to bring to foreground..");
          }
        }
      }

      // Command JOBS
      else if (!strcmp("jobs", args[0])) {
        if (head_job == NULL) {
          printf("No job is running in background.\n");
        } else {
          while (copy_head_job != NULL) {
            printf("[%d]  PID: %d \n",copy_head_job->number, copy_head_job->pid);
            copy_head_job = copy_head_job->next;
          }
        }
      }

      // Command PWD
      else if (!strcmp("pwd", args[0])) {
        myexecvp(args, &output_redirect);
      }


      // Command EXIT
      else if (!strcmp("exit", args[0])) {
  		    exit(0);
      }

      // ANY OTHER COMMANDS
      else {
        if(myexecvp(args, &output_redirect) < 0){
          printf("No command found");
        }
      }

      free(line);
    }
	}

  return 0;
}
