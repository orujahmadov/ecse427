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

struct reservation {
  char *name;
  int table_number;
};

struct reservation reservations[20];

//declaring semaphores names for local usage
sem_t *mutexA;
sem_t *mutexB;

int find_available_table(struct reservation *all_reservations[], char section) {
  int available_table;
  if (section == 'A') { // Update available table number for section A
    int available_table_A = 100;
    for (int i=0; i < 10; i++) {
      if (all_reservations[i]->table_number == available_table_A) {
        available_table_A+=1;
      }
    }
    available_table = available_table_A;
  } else { // Update available table number for section B
    int available_table_B = 200;
    for (int i=10; i < 20; i++) {
      if (all_reservations[i]->table_number == available_table_B) {
        available_table_B+=1;
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
  sem_wait(mutexA);
  sem_wait(mutexB);

  for (int i = 0; i < 20; i++) {
		all_reservations[i]->name = NULL;
    all_reservations[i]->table_number = 0;
	}

  // Signal semaphores for other processes
  sem_signal(mutexA);
  sem_signal(mutexB);

}

void status(struct reservation *all_reservations[]) {
  // Wait for semaphore signals
  sem_wait(mutexA);
  sem_wait(mutexB);

  for (int i = 0; i < 20; i++) {
    if (all_reservations[i]->table_number != 0) {
      printf("Table number %d is reserved by %s\n ", all_reservations[i]->table_number, all_reservations[i]->name);
    }
	}

  // Signal semaphores for other processes
  sem_signal(mutexA);
  sem_signal(mutexB);

}

void reserve(struct reservation *all_reservations[], char *name[], char section, int table_number) {
  // Wait for semaphore signals
  sem_wait(mutexA);
  sem_wait(mutexB);

  // If table number is given, reserve specific table
  if (table_number != 0) {
    if (section == 'A') {
      if (all_reservations[table_number - 100]->table_number != 0) {
        all_reservations[table_number - 100]->table_number = table_number;
        all_reservations[table_number - 100]->name = *name;
        printf("Table number %d is now reserved for %s\n", table_number, *name);
      } else {
        printf("Table number %d is already taken. Please select different table number.", table_number);
      }
    }
    else {
      if (all_reservations[table_number - 190]->table_number != 0) {
        all_reservations[table_number - 190]->table_number = table_number;
        all_reservations[table_number - 190]->name = *name;
        printf("Table number %d is now reserved for %s\n", table_number, *name);
      } else {
        printf("Table number %d is already taken. Please select different table number.", table_number);
      }
    }
  } else { // Reserve first available seat
    if (section == 'A') {
      int available_table_A = find_available_table(NULL,'A');
      all_reservations[available_table_A - 190]->table_number = available_table_A;
      all_reservations[available_table_A - 190]->name = *name;
      printf("Table number %d is now reserved for %s\n", available_table_A, *name);
    } else {
      int available_table_B = find_available_table(NULL,'B');
      all_reservations[available_table_B - 190]->table_number = available_table_B;
      all_reservations[available_table_B - 190]->name = *name;
      printf("Table number %d is now reserved for %s\n", available_table_B, *name);
    }
  }

  // Signal semaphores for other processes
  sem_signal(mutexA);
  sem_signal(mutexB);

}

int main() {
  char *args[20];
  char *line;
	while (1) {
		printf("%s", "$>>");
    line = read_line();
    getcmd(line, args);
    //printf(args[0]);
  }
}
