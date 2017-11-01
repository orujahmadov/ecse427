#include <semaphore.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define BUFF_SHM "/OS_BUFF"
#define BUFF_MUTEX_A "/OS_MUTEX_A"
#define BUFF_MUTEX_B "/OS_MUTEX_B"
//#define MAX_NAME_SIZE 50

struct reservation {
  char person_name[50];
  int table_number;
};

//declaring semaphores names for local usage
sem_t *mutexA;
sem_t *mutexB;

int find_available_table(struct reservation *all_reservations[], char section) {
  int available_table;
  if (section == 'A') { // Update available table number for section A
    int available_table_A = 100;
    for (int i = 0; i < 10; i++) {
      if (all_reservations[i] != NULL) {
          struct reservation r = *all_reservations[i];
          if (r.table_number == available_table_A) {
            available_table_A+=1;
          }
      }
    }
    available_table = available_table_A;
  } else { // Update available table number for section B
    int available_table_B = 200;
    for (int i = 10; i < 20; i++) {
      if (all_reservations[i] != NULL) {
          struct reservation r = *all_reservations[i];
          if (r.table_number == available_table_B) {
            available_table_B+=1;
          }
      }
    }
    available_table = available_table_B;
  }
  return available_table;
}

char *read_line(void) {
  char *line = NULL;
  ssize_t bufsize = 0; // have getline allocate a buffer for us
  getline(&line, &bufsize, stdin);

  return line;
}

/*
  Function: To reset arguments to NULL.

    args: Array of tokenized arguments list.
*/
void clean_arguments(char *args[]) {
	for (int i = 0; i < 20; i++) {
		args[i] = NULL;
	}
}

// /*
//   Function: To get command line input of user.
//
//     line: full command line entered by user.
//     args: Array of tokenized arguments list.
//     background: To specify if background is present.
//     output_redirect: To specify if output is present.
//
//   returns: Number of arguments entered in shell.
// */
int getcmd(char *line, char *args[])
{
	int i = 0;
	char *token;

	//Copy the line to a new char array because after the tokenization a big part of the line gets deleted since the null pointer is moved
	char *copyCmd = malloc(sizeof(char) * sizeof(line) * strlen(line));
	sprintf(copyCmd, "%s", line);

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

void initialize(struct reservation *all_reservations[]) {

  // Wait for semaphore signals
  // sem_wait(mutexA);
  // sem_wait(mutexB);

  for (int i = 0; i < 20; i++) {
    if (all_reservations[i]) {
      all_reservations[i] = NULL;
    }
	}
  // Signal semaphores for other processes
  // sem_signal(mutexA);
  // sem_signal(mutexB);
  printf("%s\n","All reservations are initialized.");
}

void status(struct reservation *all_reservations[]) {
  // Wait for semaphore signals
  // sem_wait(mutexA);
  // sem_wait(mutexB);
  for (int i = 0; i < 20; i++) {
    if (all_reservations[i] != NULL) {
        struct reservation r = *all_reservations[i];
        printf("Table number %d is reserved by %s\n", r.table_number, r.person_name);
    }
	}

  // Signal semaphores for other processes
  // sem_signal(mutexA);
  // sem_signal(mutexB);

}

void reserve(struct reservation *all_reservations[], char name[], char *section[], int table_number) {
  // Wait for semaphore signals
  // sem_wait(mutexA);
  // sem_wait(mutexB);

  if (!strcmp(*section, "A")) {
    if (table_number == -1) {
      table_number = find_available_table(all_reservations,'A');
    }
    if (all_reservations[table_number - 100] != NULL) {
      printf("Table number %d is already taken. Please select different table number.\n", table_number);
    }
    else {
      struct reservation *new_reservation = malloc(sizeof(struct reservation));
      strcpy(new_reservation->person_name, name);
      new_reservation->table_number = table_number;
      all_reservations[table_number - 100] = new_reservation;
      printf("Table number %d in section %s is now reserved for %s\n", table_number, *section, name);
    }
  }
  else {
    if (table_number == -1) {
      table_number = find_available_table(all_reservations,'B');
    }
    if (all_reservations[table_number - 190] != NULL) {
      printf("Table number %d is already taken. Please select different table number.", table_number);
    }
    else {
      struct reservation *new_reservation = malloc(sizeof(struct reservation));
      strcpy(new_reservation->person_name, name);
      new_reservation->table_number = table_number;
      all_reservations[table_number - 190] = new_reservation;
      printf("Table number %d in section %s is now reserved for %s\n", table_number, *section, name);
    }
  }

  // Signal semaphores for other processes
  // sem_signal(mutexA);
  // sem_signal(mutexB);

}

int check_command(char *args[]) {
  int flag = 0;
  if (args[0] == NULL) {
    flag = -1;
    printf("%s\n","Invalid command.");
  } else {
    if (!strcmp("reserve", args[0])) {
      if (args[1] == NULL) {
        flag = -1;
        printf("%s\n","Please enter person name.");
      } else {
        if (args[2] == NULL || (strcmp("A", args[2]) !=0 && strcmp("B", args[2]) !=0)) {
          flag = -1;
          printf("%s\n","Invalid section.");
        } else {
          if (args[3] != NULL) {
            int table_number = atoi(args[3]);
            if (!strcmp("A", args[2])) {
              if (table_number < 100 || table_number >= 110) {
                flag = -1;
                printf("%s\n","Invalid table number.");
              }
            } else {
              if (table_number < 200 || table_number >= 210) {
                flag = -1;
                printf("%s\n","Invalid table number.");
              }
            }
          }
        }
      }
    }
  }
  return flag;
}

int main() {
  char *args[20];
  struct reservation *reservations[20];
  char *line;
  size_t linecap = 0; // 16 bit unsigned integer
  int length = 0;

	while (1) {
    /*       Reset Variables      */
    clean_arguments(args);

		printf("%s", "$>>");
    length = getline(&line, &linecap, stdin);
    int cnt = getcmd(line, args);
    if (cnt != 0) {
      if (check_command(args) != -1) {
        // Command Reserve
        if (!strcmp("reserve", args[0])) {
          int table_number = -1;
          if (args[3] != NULL) table_number = atoi(args[3]);
          reserve(reservations, args[1], &args[2], table_number);
        }
        // Command Initialize
        else if (!strcmp("init", args[0])) {
          initialize(reservations);
        }
        // Command Status
        else if (!strcmp("status", args[0])) {
          status(reservations);
        }
        // Command Exit
        else if (!strcmp("exit", args[0])) {
          exit(0);
        }
      }
    }
  }
}
