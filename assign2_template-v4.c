/*
  To compile assign2_template-v3.c ensure that gcc is installed and run
  the following command:

  gcc your_program.c -o your_ass-2 -lpthread -lrt -Wall
*/

#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/* to be used for your memory allocation, write/read. man mmsp */
#define SHARED_MEM_NAME "/my_shared_memory"
#define SHARED_MEM_SIZE 1024

/* --- Structs --- */
typedef struct ThreadParams {
  int pipeFile[2]; // [0] for read and [1] for write. use pipe for data transfer
                   // from thread A to thread B
  sem_t sem_A, sem_B, sem_C; // the semphore
  char message[255];
  char inputFile[100];  // input file name
  char outputFile[100]; // output file name
} ThreadParams;

/* Global variables */
pthread_attr_t attr;

int shm_fd; // use shared memory for data transfer from thread B to Thread C

/* --- Prototypes --- */

/* Initializes data and utilities used in thread params */
void initializeData(ThreadParams *params);

/* This thread reads data from data.txt and writes each line to a pipe */
void *ThreadA(void *params);

/* This thread reads data from pipe used in ThreadA and writes it to a shared
 * variable */
void *ThreadB(void *params);

/* This thread reads from shared variable and outputs non-header text to src.txt
 */
void *ThreadC(void *params);

/* --- Main Code --- */
int main(int argc, char const *argv[]) {
  pthread_t tid[3]; // three threads
  ThreadParams params;

  if (argc != 3) {
    fprintf(stderr, "USAGE:./main data.txt output.txt\n");
    exit(0);
  }

  // Initialization our parameters
  initializeData(&params);

  // this will get the input file names and the output filename
  // so we can use them in the thread params to read and write the
  // relevant data
  strcpy(params.inputFile, argv[1]);
  strcpy(params.outputFile, argv[2]);

  // Create Threads and run them simulationously
  pthread_create(&(tid[0]), &attr, &ThreadA, (void *)(&params));
  pthread_create(&(tid[1]), &attr, &ThreadB, (void *)(&params));
  pthread_create(&(tid[2]), &attr, &ThreadC, (void *)(&params));

  // Wait for threads before closing program
  pthread_join(tid[0], NULL);
  pthread_join(tid[1], NULL);
  pthread_join(tid[2], NULL);

  return 0;
}

void initializeData(ThreadParams *params) {
  // Initialize Sempahores
  // This initialize is important here, we initialize semaphorm A
  // unlocked so we can initialize the program execution
  if (sem_init(&(params->sem_A), 0, 1) != 0) { // Set up Sem for thread A
    perror("error for init threa A");
    exit(1);
  }
  if (sem_init(&(params->sem_B), 0, 0) != 0) { // Set up Sem for thread B
    perror("error for init threa B");
    exit(1);
  }
  if (sem_init(&(params->sem_C), 0, 0) != 0) { // Set up Sem for thread C
    perror("error for init threa C");
    exit(1);
  }

  // Initialize thread attributes
  pthread_attr_init(&attr);

  // Create a Read & Write pipe that will be used
  // for the communicaiton from Thread A to Thread B
  if (pipe(params->pipeFile) == -1) {
    perror("Pipe failed");
    exit(1);
  }

  // Setup share memory for Thread B & C
  shm_fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0666);
  ftruncate(shm_fd, SHARED_MEM_SIZE);
}

void *ThreadA(void *params) {
  ThreadParams *thread_params = (ThreadParams *)params;

  FILE *file = fopen(thread_params->inputFile, "r");
  if (!file) {
    perror("Failed to open input file");
    pthread_exit(NULL);
  }

  // Initialize our line buffer to read in
  // the line from the file
  char buffer[255];

  // This loop will continue until there are no
  // more lines to read
  while (fgets(buffer, sizeof(buffer), file)) {
    // This is the first semaphore wait
    // in our program, this semaphore is initially
    // unlocked to start the program logic, more can be seen
    // on our flowchart
    sem_wait(&thread_params->sem_A);

    // We write the line that we have scanned in from the
    // file into the file pipe where it can be read by ThreadB
    write(thread_params->pipeFile[1], buffer, strlen(buffer) + 1);

    // We finally post to semaphore B unlocking it
    // so thread B can start its next iteration
    sem_post(&thread_params->sem_B);
  }

  // Since we have completed processing
  // the file, we want to send a termination
  // value to the other threads to inform them that there
  // will be no more incoming lines

  // Same as before we wait until its our turn
  // to process by waiting till our semaphore is unlocked
  // in thread C
  sem_wait(&thread_params->sem_A);

  // We write the __EOF__ flag that will tell ThreadB that
  // we have completed processing our file
  write(thread_params->pipeFile[1], "__EOF__", 8);

  // We then unlock ThreadB semaphore for it to start
  // its next and final iteration
  sem_post(&thread_params->sem_B);

  // We will close our open stream for the file
  fclose(file);

  // Finally we will exit the program
  pthread_exit(NULL);
}

void *ThreadB(void *params) {
  ThreadParams *thread_params = (ThreadParams *)params;
  char buffer[255];

  // Open shared memory in Write mode which will be used
  // to communicate with Thread C
  char *shared_mem =
      mmap(0, SHARED_MEM_SIZE, PROT_WRITE, MAP_SHARED, shm_fd, 0);

  // Initialize our loop here which
  // will read the file pipe until __EOF__ is
  // read
  while (1) {
    // Wait until semaphore B is unlocked by Thread A
    sem_wait(&thread_params->sem_B);

    // Read in our file line from the pipe that has been written
    // to by Thread A and put it into the buffer variable
    read(thread_params->pipeFile[0], buffer, sizeof(buffer));

    // Copy that buffer into shared memory where it
    // can be read by Thread C
    strcpy(shared_mem, buffer);

    // Unlock Semaphore C for Thread C to start its execution
    sem_post(&thread_params->sem_C);

    // Listen to the __EOF__ call from Thread A to stop
    // exeucting our loop here
    if (strcmp(buffer, "__EOF__") == 0) {
      break;
    }
  }

  close(shm_fd);

  // Unmap the shared memory because it is going to be unlinked soon
  munmap(shared_mem, SHARED_MEM_SIZE);

  // Close the thread
  pthread_exit(NULL);
}

void *ThreadC(void *params) {
  ThreadParams *thread_params = (ThreadParams *)params;

  // Open output file in Write mode preparing it for output
  FILE *output_file = fopen(thread_params->outputFile, "w");
  if (!output_file) {
    perror("Failed to open output file");
    pthread_exit(NULL);
  }

  // Create a reference to our shared memory in read mode
  char *shared_mem = mmap(0, SHARED_MEM_SIZE, PROT_READ, MAP_SHARED, shm_fd, 0);
  // Create a tracking variable for if the header has been found
  int found_header = 0;

  while (1) {
    // Wait for the Semaphore C to be unlocked to start processing
    sem_wait(&thread_params->sem_C);

    // Listen for __EOF__ from Thread A to
    // indicate the program has completed reading from
    // the file
    if (strcmp(shared_mem, "__EOF__") == 0) {
      break;
    }

    // We check if the header is still being read
    // we can track that by the boolean value we create
    // named header_done
    if (!found_header) {
      // Check if it the end of the header to set the value
      if (strstr(shared_mem, "end_header")) {
        found_header = 1;
      }
      // Unblock ThreadA to work again
      sem_post(&thread_params->sem_A);
      continue;
    }

    // 2nd page of assignement wanted
    // us to print shared memory to the
    // console / monitor
    printf("%s", shared_mem);

    // Since we have finished reading the header
    // we can now write to file
    fputs(shared_mem, output_file);
    // Unblock threada to work again
    sem_post(&thread_params->sem_A);
  }

  // Close the output file so all the operations are written to disk
  fclose(output_file);

  // Unmap the shared memory because it is going to be unlinked soon
  munmap(shared_mem, SHARED_MEM_SIZE);

  // Unlink the shared memory as this is the end of the program
  shm_unlink(SHARED_MEM_NAME);

  pthread_exit(NULL);
}
