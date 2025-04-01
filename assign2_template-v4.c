/***********************************************************************************/
//***********************************************************************************
//            *************NOTE**************
// This is a template for the subject of RTOS in University of Technology Sydney(UTS)
// Please complete the code based on the assignment requirement.

//***********************************************************************************
/***********************************************************************************/

/*
  To compile assign2_template-v3.c ensure that gcc is installed and run 
  the following command:

  gcc your_program.c -o your_ass-2 -lpthread -lrt -Wall
*/

#include  <pthread.h>
#include  <stdlib.h>
#include  <unistd.h>
#include  <stdio.h>
#include  <sys/types.h>
#include  <fcntl.h>
#include  <string.h>
#include  <sys/stat.h>
#include  <semaphore.h>
#include  <sys/time.h>
#include  <sys/mman.h>

/* to be used for your memory allocation, write/read. man mmsp */
#define SHARED_MEM_NAME "/my_shared_memory"
#define SHARED_MEM_SIZE 1024

/* --- Structs --- */
typedef struct ThreadParams {
  int pipeFile[2]; // [0] for read and [1] for write. use pipe for data transfer from thread A to thread B
  sem_t sem_A, sem_B, sem_C; // the semphore
  char message[255];
  char inputFile[100]; // input file name
  char outputFile[100]; // output file name
} ThreadParams;

/* Global variables */
int sum = 1;

pthread_attr_t attr;

int shm_fd;// use shared memory for data transfer from thread B to Thread C 

/* --- Prototypes --- */

/* Initializes data and utilities used in thread params */
void initializeData(ThreadParams *params);

/* This thread reads data from data.txt and writes each line to a pipe */
void* ThreadA(void *params);
/* This thread reads data from pipe used in ThreadA and writes it to a shared variable */
void* ThreadB(void *params);
/* This thread reads from shared variable and outputs non-header text to src.txt */
void* ThreadC(void *params);

/* --- Main Code --- */
int main(int argc, char const *argv[]) {
  
 pthread_t tid[3]; // three threads A = 0 , B = 1 , C = 2
 ThreadParams params;
 
  
  // Initialization
  initializeData(&params);



  // Create Threads
  pthread_create(&(tid[0]), &attr, &ThreadA, (void*)(&params));

  //TODO: add your code
  pthread_create(&tid[1], &attr , &ThreadB, (void*)(&params));
  pthread_create(&tid[2], &attr , &ThreadC, (void*)(&params));
 

  // Wait on threads to finish
  pthread_join(tid[0], NULL);
  pthread_join(tid[1], NULL);
  pthread_join(tid[2], NULL);
  
    
  //TODO: add your code

  return 0;
}

void initializeData(ThreadParams *params) {
  // Initialize Sempahores
  if(sem_init(&(params->sem_A), 0, 1) != 0) { // Set up Sem for thread A
    perror("error for init threa A");
    exit(1);
  }
if(sem_init(&(params->sem_B), 0, 0) != 0) { // Set up Sem for thread B
    perror("error for init threa B");
    exit(1);
  }
  if(sem_init(&(params->sem_C), 0, 0) != 0) { // Set up Sem for thread C
    perror("error for init threa C");
    exit(1);
  } 

// Initialize thread attributes 
  pthread_attr_init(&attr);
  //TODO: add your code
  if (pipe(params->pipeFile) < 0){
    perror("Create pipe fail");
    exit(1);
  }

  strcpy(params -> inputFile , "data.txt"); // we pass in the input file name
  strcpy(params -> outputFile, "output.txt"); //we pass in the output file name

  //We start initialize the semaphores here
  //sem_init(sem_t *sem, int pshared, unsigned int value);
  //for p shared:
  //0: shared between threads in the same process
  //1: shared between threads in different processes

  //and for int value :
  //0: semaphore is locked
  //1: semaphore is unlock

  //we set up the shared memory here
  shm_fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR , 0666); //set up the shared memory
  ftruncate(shm_fd , SHARED_MEM_SIZE); // resize the Shared memory to the size we want

  return;
}

void* ThreadA(void *params) {
  //TODO: add your code
  ThreadParams* p = (ThreadParams*) params; //cast a void pointer to struc type
  FILE* fp = fopen(p -> inputFile, "r"); //open the input file in read mode
  char line[255]; // buffer to read each line from file
  if(!fp) {
    perror("Failed to open input file");
    pthread_exit(NULL);
  }

  while(fgets(line, 255, fp)){
    sem_wait(&p -> sem_A); //wait for Sem_A turn
    write(p -> pipeFile[1], line , strlen(line) + 1); //Write the lione to the pipe
    sem_post(&p -> sem_B); //give signal to sem_B to start
  }

strcpy(line, "__EOF__"); //we add the EOF so the thread B now when to stop
sem_wait(&p->sem_A);
write(p->pipeFile[1], line, strlen(line) + 1);
sem_post(&p->sem_B);

  fclose(fp); //we close the fp aka file after finish reading it
  
  printf("Thread A: sum = %d\n", sum);
  pthread_exit(NULL); //end the thread cleany
}

void* ThreadB(void *params) {
  //TODO: add your code
  ThreadParams* p = (ThreadParams*) params; //cast a void pointer to struc type
  char line[255]; // buffer to read each line from pipe

  //We first map the shared memory for writing
  char* shm_ptr = mmap(0, SHARED_MEM_SIZE , PROT_WRITE,MAP_SHARED, shm_fd,0);

  while(1){ //keep looping until it reach the end of the file
    sem_wait(&p -> sem_B); //we wait for the data from thread A to send
    read(p -> pipeFile[0], line , sizeof(line)); //start read the data from the pipe
    strcpy(shm_ptr , line ); //we copy the line to the memory
    sem_post(&p -> sem_C); //give signal to sem_C to start

    if(strcmp(line, "__EOF__") == 0){
      break; // we reach the end of the file, we break the loop
    }
  }

  printf("Thread B: sum = %d\n", sum);
  pthread_exit(NULL);
}

void* ThreadC(void *params) {
  //TODO: add your code
  ThreadParams* p = (ThreadParams*) params; //cast a void pointer to struc type
  FILE* fp = fopen(p -> outputFile , "w"); //open the file in write mode

  //We use the map shared memory for reading
  char* shm_ptr = mmap(0 , SHARED_MEM_SIZE , PROT_READ , MAP_SHARED ,shm_fd , 0);

  int header_end = 0; //use for skip the header until reaching "end_header"
  //use the strcmp to compare the string to find the EOF and header
  while(1){
    sem_wait(&p -> sem_C); //wait for sem_B to finish
    if (strcmp(shm_ptr, "__EOF__") == 0){
      break;
    } 
    
    if(header_end){ //shm_ptr will point to the memory where the actual data from threadB write on
      fputs(shm_ptr , fp); //Write the content down onto output File
    }
    
    else if (strstr(shm_ptr , "end_header") != NULL){
      header_end = 1; // we found the end header so we mark it
    }
    sem_post(&p -> sem_A); //we give signal to sem A to continue 
  }

  fclose(fp); //close the file after finish
  shm_unlink(SHARED_MEM_NAME); //clean up the share memory after finish


 printf("Thread C: Final sum = %d\n", sum);
 pthread_exit(NULL); //end the thread cleany
}
