// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
 #define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/select.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>

// Defines:
// Amount of childs to create to handle the files.
#define NUMBER_OF_CHILDS 5
// Ends the proccess if an error ocurred
#define CHECK_ERROR_NEGATIVE(error_value, function_name) if(error_value == -1) { perror(function_name);  exit(EXIT_FAILURE);}
// Max length of the paths of all files sent to each child
#define MAX_LENGTH_OF_SENDING_FILES  500
 // for distinguishing different outputs to children
#define DELIMITER "~"
#define DELIMITER_CHARACTER '~'
#define MAX_OUTPUT_EXPECTED_FROM_CHILD 100

// Functions Declarations:
static void hear_answers_and_give_more_files_to_chidren(int fd_to_read[], int fd_to_write[], 
                                                        const char* argv[], size_t* nextFile, int startingFiles, 
                                                        char* write_to_shm, sem_t* files_to_print, FILE* result_file);
static void send_n_files_to_child(int child_fd, int n, const char* argv[], size_t* current_position_argv);
static int max_fd(int fd_vec[]);
static int amount_of_files_this_child_completed(char child_output[]);
static void initialize_shared_memory(int argc, int* shm_size, char** write_to_shm);
static void create_pipes(int * pipes_master_to_slave, int * pipes_slave_to_master);
static void close_every_resource_used(FILE ** result_file, sem_t ** files_to_print, char ** write_to_shm, int shm_size);
static void close_all_pipes(int * pipes_master_to_slave, int * pipes_slave_to_master);

int error = 0; // error: Is used to check every possible integer-value error
pid_t children_pid[NUMBER_OF_CHILDS]; // childen_pic: Sorted by order of created children

int main (int argc, const char * argv []) {
    if(argc < 2) {          // Check for correct usage
        fprintf(stderr, "At least one CNF file must be given\n");
        exit(EXIT_FAILURE); 
    }

    size_t nextFile = 1;     // Index in argv of the path to the next file to be sent to slaves
       
    setvbuf(stdout, NULL, _IONBF, 0);       // Here we disable the buffer so the texts we prints can go directly to the STDOUT
    
    size_t startingFiles = argc/50 + 2;     // Starting amount of files sent to each slave formula

    FILE * result_file = fopen("resultado", "w");    // opens the "resultado" file to write everything in it

    int shm_size;           // size of the shared memory
    char * write_to_shm;     // pointer to the data in the shared memory
    initialize_shared_memory(argc, &shm_size, &write_to_shm);

    sem_t * files_to_print = sem_open("/files_to_print", O_CREAT, 0666, 0);     // Semaphore for dealing correctly with the Shared Memory

    printf("%d\n", argc - 1);        // send to STDOUT the amount of files to proccess
    
    sleep(2);                        // wait for a VIEW proccess to appear

    int pipes_master_to_slave[NUMBER_OF_CHILDS][2];
    int pipes_slave_to_master[NUMBER_OF_CHILDS][2];

    // the following 2 arrays are used so that the parent can easily know WHERE to write, and from WHERE to listen
    int fd_pipe_master_to_slave_vec[NUMBER_OF_CHILDS];      // sorted by order of created children
    int fd_pipe_slave_to_master_vec[NUMBER_OF_CHILDS];      // sorted by order of created children

    create_pipes(pipes_master_to_slave[0], pipes_slave_to_master[0]);
    
    /**** CREATE THE CHILDREN AND SEND THEM THE INITIAL AMOUNT OF FILES ****/
    size_t i;
    pid_t pid;
    for (i = 0; i < NUMBER_OF_CHILDS; i++) {
        pid = fork();
        CHECK_ERROR_NEGATIVE(pid, "fork");
        if( pid == 0 ){ // i'm a child
            error = close(fileno(stdin));                    // close STDIN
            CHECK_ERROR_NEGATIVE(error, "close STDIN");
            error = dup(pipes_master_to_slave[i][0]);        // new STDIN: read-end of master-to-slave pipe N°i
            CHECK_ERROR_NEGATIVE(error, "dup");
            error = close(fileno(stdout));                   // close STDOUT
            CHECK_ERROR_NEGATIVE(error, "close STDOUT");
            error = dup(pipes_slave_to_master[i][1]);        // new STDOUT: write-end of slave-to-master pipe N°i
            CHECK_ERROR_NEGATIVE(error, "dup");

            close_all_pipes(&pipes_master_to_slave[0][0], &pipes_slave_to_master[0][0]);

            char *newenviron[] = { NULL };
            char *newargv[] = { "slave", NULL };
            error = execve("slave", newargv, newenviron);
            CHECK_ERROR_NEGATIVE(error, "execve");
        } else { // i'm a father
            children_pid[i] = pid;
            fd_pipe_master_to_slave_vec[i] = dup(pipes_master_to_slave[i][1]);   // create a new FD, representing the write-end of
            CHECK_ERROR_NEGATIVE(fd_pipe_master_to_slave_vec[i], "dup");         // master-to-slave pipe N°i
            fd_pipe_slave_to_master_vec[i] = dup(pipes_slave_to_master[i][0]);   // create a new FD, representing the read-end of
            CHECK_ERROR_NEGATIVE(fd_pipe_slave_to_master_vec[i], "dup");         // slave-to-master pipe N°i

            //  Start distributing the initial files
            int actual_sending_files = (argv[nextFile+1] == NULL)? 1 : startingFiles;
            send_n_files_to_child(fd_pipe_master_to_slave_vec[i], actual_sending_files, argv, &nextFile);
        }
    }

    /**** after creating children and giving them the initial N° of files ****/
    hear_answers_and_give_more_files_to_chidren(fd_pipe_slave_to_master_vec, fd_pipe_master_to_slave_vec, argv, &nextFile, 
                                                startingFiles, write_to_shm, files_to_print, result_file);
    
    close_all_pipes(&pipes_master_to_slave[0][0], &pipes_slave_to_master[0][0]);

    close_every_resource_used(&result_file, &files_to_print, &write_to_shm, shm_size);

    return 0;
}

static void hear_answers_and_give_more_files_to_chidren(int fd_to_read[], int fd_to_write[], const char* argv[], 
                                                        size_t* nextFile, int sf, char* write_to_shm, 
                                                        sem_t* files_to_print, FILE* result_file) {
    
    size_t children_left_to_finish = NUMBER_OF_CHILDS;  // when this gets down to 0, this function must return.
    fd_set readfds;      // readable-fds-set
    FD_ZERO(&readfds);   // start the readable-fds-set

    int retval;  // used by the select syscall

    /* sorted by order of created children. 
    Purpose 1: prevent parent from giving a child a new file when the child has only answered a fraction of all initial files
    Purpose 2: prevent the parent from killing the child if it has files in execution */
    int starting_files_left_to_finish[NUMBER_OF_CHILDS];
    size_t i;
    for(i=0; i<NUMBER_OF_CHILDS; i++) 
        starting_files_left_to_finish[i] = sf;

    while(children_left_to_finish > 0) {

        for(int i=0; i<NUMBER_OF_CHILDS; i++) 
            FD_SET(fd_to_read[i], &readfds);   // always prepare the readable-fds-set for reading

        retval = select( max_fd(fd_to_read) + 1, &readfds, NULL, NULL, NULL ); // blocks until a child has something to write to parent
        CHECK_ERROR_NEGATIVE(retval, "select");

        size_t k;
        for(k=0; retval>0 && k<NUMBER_OF_CHILDS; k++) {  // iterate over all children...
            if( FD_ISSET(fd_to_read[k], &readfds) ) {    // ... but only read over the children who have written to master
                
                char answer_from_child[MAX_OUTPUT_EXPECTED_FROM_CHILD] = {'\0'};
                read(fd_to_read[k], answer_from_child, MAX_OUTPUT_EXPECTED_FROM_CHILD);   // read from child who has something to write to parent
                
                strcat(write_to_shm, answer_from_child);   // print the output to the Shared Memory
                sem_post(files_to_print);
                
                fprintf(result_file, "%s\n", answer_from_child);  // also print the output to the result file

                retval--;

                int files_completed = amount_of_files_this_child_completed(answer_from_child);
                starting_files_left_to_finish[k] -= files_completed;

                if(argv[ *nextFile ] != NULL){    // send to the child that has just written to parent a new file only if there are any
                    if( starting_files_left_to_finish[k] <= 0 )  // only send new file to this child if it hasn't something left to write to parent
                        send_n_files_to_child(fd_to_write[k], 1, argv, nextFile);

                } else {                                         // kill the child if there are no new files to send...
                    if(starting_files_left_to_finish[k] <= 0){   // ... but also if it has finished proccessing its own files
                        children_left_to_finish--;
                        kill(children_pid[k], SIGKILL);   // kill child because we have anything else to give to it
                        int status;
                        waitpid(children_pid[k], &status, 0);
                    }
                }
            }
        }
    }
}

/* this function also takes care of iterating over ARGV[] of the main function */
static void send_n_files_to_child(int child_fd, int n, const char* argv[], size_t* current_position_argv) {
    int i;
    char sending_files[MAX_LENGTH_OF_SENDING_FILES] = {'\0'};
    for(i = 0; i < n; i++) {
        //printf("Master envio %s al hijo con FD %d y posicion en argv = %zu\n", argv[*current_position_argv], child_fd, *current_position_argv);
        strcat(sending_files, argv[ (*current_position_argv) ]);
        (*current_position_argv)++;     // here is where we iterate over ARGV[]
        strcat(sending_files, DELIMITER);  // concat a DELIMITER for distinguishing files from each other
    }
    dprintf(child_fd, "%s", sending_files);    // send text to child, using its File Descriptor
} 

/* just to obtain the max File Descriptor for the select syscall */
static int max_fd(int fd_vec[]) {
    int max = fd_vec[0];
    int i;
    for(i=1; i<NUMBER_OF_CHILDS; i++) {
        if(fd_vec[i] > max) 
            max = fd_vec[i];
    }
    return max;
}

/* check how many '~' were found in the child's response (1 '~' ---> 1 file was proccessed) */
static int amount_of_files_this_child_completed(char child_output[]) {
    int i, amount = 0;
    for(i=0; child_output[i] != '\0'; i++) {
        if(child_output[i] == DELIMITER_CHARACTER)
            amount++;
    }
    return amount;
}

static void initialize_shared_memory(int argc, int* shm_size, char** write_to_shm) {
    *shm_size = (argc - 1) * MAX_OUTPUT_EXPECTED_FROM_CHILD;  // size of shared memory

    int shm_fd = shm_open("/super_shared_memory", O_CREAT | O_EXCL | O_RDWR, 0666);  
    CHECK_ERROR_NEGATIVE(shm_fd, "shm_open from MASTER");

    error = ftruncate(shm_fd, *shm_size);
    CHECK_ERROR_NEGATIVE(error, "ftruncate");

    // pointer to write to the Shared Memory
    *write_to_shm = (char*) mmap(NULL, *shm_size, PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if( *write_to_shm == (void*)(-1) ) 
    { perror("mmap from MASTER"); exit(EXIT_FAILURE); }
    error = close(shm_fd);   // after a successful call to mmap, the FD can be closed without affecting memory mapping
    CHECK_ERROR_NEGATIVE(error, "close SHM from MASTER");
}

static void create_pipes(int * pipes_master_to_slave, int * pipes_slave_to_master) {
    size_t j;
    for(j=0; j<NUMBER_OF_CHILDS; j++) {       // create all the pipes needed
        error = pipe( pipes_master_to_slave + 2 * j );
        CHECK_ERROR_NEGATIVE(error, "pipe");
        error = pipe( pipes_slave_to_master + 2 * j );
        CHECK_ERROR_NEGATIVE(error, "pipe");
    }
}

static void close_every_resource_used(FILE ** result_file, sem_t ** files_to_print, char ** write_to_shm, int shm_size) {
    error = fclose(*result_file);
    CHECK_ERROR_NEGATIVE(error, "close");

    error = sem_close(*files_to_print);
    CHECK_ERROR_NEGATIVE(error, "sem_close in MASTER");
    error = sem_unlink("/files_to_print");
    CHECK_ERROR_NEGATIVE(error, "sem_unlink");

    error = munmap((void*) * write_to_shm, shm_size);
    CHECK_ERROR_NEGATIVE(error, "munmap from MASTER");
    error = shm_unlink("/super_shared_memory");
    CHECK_ERROR_NEGATIVE(error, "shm_unlink from MASTER");
}

static void close_all_pipes(int * pipes_master_to_slave, int * pipes_slave_to_master) {
    size_t j;
    for (j=0; j < NUMBER_OF_CHILDS; j++) {     // close all pipes in every child
        close( * (pipes_master_to_slave + 2 * j) );
        close( * (pipes_master_to_slave + 2 * j + 1) );
        close( * (pipes_slave_to_master + 2 * j) );
        close( * (pipes_slave_to_master + 2 * j + 1) );
    }
}