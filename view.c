// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <stdio.h>
#include <sys/mman.h>
//#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <errno.h>

#define CHECK_ERROR_NEGATIVE(error_value, function_name) if(error_value == -1) { perror(function_name);  exit(EXIT_FAILURE);}
#define MAX_OUTPUT_GIVEN_BY_MASTER 100

int error = 0;

int main(int argc, char* argv[]) {
    int number_of_files_to_proccess;

    setvbuf(stdout, NULL, _IONBF, 0);

    if(argc > 2) {
        fprintf(stderr, "Must receive 0 or 1 argument only\n");
        exit(EXIT_FAILURE);
    } else if(argc == 2){ // in case this View proccess receives the information from command line
        number_of_files_to_proccess = atoi( argv[1] );
        printf("view received from ARGV = %d\n", number_of_files_to_proccess);
    } else { // in case this View proccess receives the information from STDIN
        scanf("%d", &number_of_files_to_proccess);
        printf("view received from STDIN = %d\n", number_of_files_to_proccess);
    }

    int shm_size = number_of_files_to_proccess * MAX_OUTPUT_GIVEN_BY_MASTER;

    int shm = shm_open("/super_shared_memory", O_RDONLY, 0666);
    CHECK_ERROR_NEGATIVE(shm, "shm_open from VIEW");
    char* shared_memory = (char*) mmap(NULL, shm_size, PROT_READ, MAP_SHARED, shm, 0);
    if( shared_memory == (void*)(-1) ) { 
        perror("mmap from MASTER"); 
        exit(EXIT_FAILURE); 
    }
    error = close(shm);     // after a successful call to mmap, the FD can be closed without affecting memory mapping
    CHECK_ERROR_NEGATIVE(error, "close SHM from VIEW");
    
    sem_t* files_to_print = sem_open("/files_to_print", O_RDWR);

    char to_print[300] = {'\0'};

    error = sem_wait(files_to_print);
    CHECK_ERROR_NEGATIVE(error, "sem_wait on VIEW");
    int j=0, i;

    // this for-cycle will only end when no more files will be written, because right before reaching '\0', the proccess 
    // will block, and when it resumes it will encounter new data ahead of where it stopped
    for(i = 0 ; shared_memory[i] != '\0' ; i++) {
        if( shared_memory[i] == '~' ) {
            to_print[j] = '\0';
            printf("%s\n", to_print);
            j = 0;
            number_of_files_to_proccess--;
            if(number_of_files_to_proccess > 0)  
                sem_wait(files_to_print);    // block only if there will be new files 

        } else
            to_print[j++] = shared_memory[i];
    }

    sem_close(files_to_print);
    CHECK_ERROR_NEGATIVE(error, "sem_close in VIEW");    

    error = munmap((void*) shared_memory, shm_size);
    CHECK_ERROR_NEGATIVE(error, "munmap from VIEW");

    return 0;
}
