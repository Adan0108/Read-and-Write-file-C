#include <stdio.h>
#include <string.h>
#include <stdlib.h> // For exit() function


int main(int argc, char** argv) {
    char c[1000];
    FILE *fptr;
    char file_name[100];
    int sig;
   char check[1000]="end_header";

/* Verify the correct number of arguments were passed in */
	if (argc != 2) {
		fprintf(stderr, "USAGE:./main.out data.txt\n");	
	}

    strcpy(file_name, argv[1]); //copy a string from the commond line


    if ((fptr = fopen(file_name, "r")) == NULL) {
        printf("Error! opening file");
        // Program exits if file pointer returns NULL.
        exit(1);
    }

    // reads text until newline is encountered
   printf("reading from the file:\n"); 
   
   sig=0;

   while(fgets(c, sizeof(c), fptr) != NULL) {
	     
	fputs(c, stdout);
	
     }

    fclose(fptr);

    return 0;
}
