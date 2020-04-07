// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _BSD_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>

#define CHECK_ERROR_NEGATIVE(error_value, function_name) if(error_value == -1){perror(function_name);  exit(EXIT_FAILURE);}
#define CHECK_ERROR_NULL(error_value, function_name) if(error_value == NULL){perror(function_name);  exit(EXIT_FAILURE);}
#define MINISAT_WAITING_FOR_ARGUMENTS "minisat "
#define FILTERING_MINISAT_OUTPUT " | grep -o -e 'Number of.*[0-9]\\+' -e 'CPU time.*' -e '.*SATISFIABLE' | grep -o -e '[0-9|.]*' -o -e '.*SATISFIABLE' | xargs | sed 's/ /\t/g'"
#define MAX_MINISAT_COMMAND_LENGTH 300
#define MAX_FILES_RECEIVED_LENGTH 500
#define MAX_SINGLE_FILE_LENGTH 100
#define MAX_EXPECTED_LENGTH_FROM_FILTERED_MINISAT 100
#define DELIMITER "~"
#define DELIMITER_CHARACTER '~'

void receive_files();
void fix_path(char files[], char** single_actual_file);
void execute_minisat_with_file(char file[]);
void send_result_to_parent(char output_to_parent[]);

int error = 0;

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    receive_files();
    exit(EXIT_SUCCESS);
}

void receive_files() {
    int count;
    char single_file[MAX_SINGLE_FILE_LENGTH] = {'\0'};  // each file name will be stored here

    while(1) {
        char files_received[MAX_FILES_RECEIVED_LENGTH] = {'\0'};
        count = read(STDIN_FILENO, files_received, MAX_FILES_RECEIVED_LENGTH);  // read from STDIN (master-to-slave pipe)
        CHECK_ERROR_NEGATIVE(count, "read");
        
        if(count > 0) {
            int i, j=0;
            for(i=0; files_received[i] != '\0'; i++) {   // this for-cycle will execute the files one by one
                if( files_received[i] == DELIMITER_CHARACTER ) {    // if the DELIMITER is found, then we must execute the file
                    single_file[j] = '\0';
                    execute_minisat_with_file(single_file);
                    j = 0;
                } else {
                    single_file[j++] = files_received[i];
                }
            }
        } else {
            exit(EXIT_FAILURE);
        }
    }
}

void execute_minisat_with_file(char file[]) {

    char to_execute[MAX_MINISAT_COMMAND_LENGTH] = {'\0'};  /***************/
    strcat(to_execute, MINISAT_WAITING_FOR_ARGUMENTS);    // we prepare the full command
    strcat(to_execute, file);                             // to be executed
    strcat(to_execute, FILTERING_MINISAT_OUTPUT);         /**************/

    FILE* cnf_file = popen(to_execute, "r");   // open minisat command
    CHECK_ERROR_NULL(cnf_file, "popen");

    char* p;
    char output_returned_by_filtered_minisat[MAX_EXPECTED_LENGTH_FROM_FILTERED_MINISAT] = {'\0'};
    p = fgets(output_returned_by_filtered_minisat, MAX_EXPECTED_LENGTH_FROM_FILTERED_MINISAT, cnf_file);
    CHECK_ERROR_NULL(p, "fgets"); 

    char output_to_parent[MAX_EXPECTED_LENGTH_FROM_FILTERED_MINISAT + MAX_SINGLE_FILE_LENGTH] = {'\0'};
    sprintf(output_to_parent, "%d   ", getpid());
    strcat(output_to_parent, file);
    strcat(output_to_parent, "   ");
    strcat(output_to_parent, output_returned_by_filtered_minisat);
    // we have to concat the delimiter, as sometimes slaves write outputs (from multiples files) very fast
    strcat(output_to_parent, DELIMITER);

    /* right now, output_to_parent should look similar to a multiple of this: 
       "1255   Files/uf250-010.cnf  250  1065  0.006802   UNSATISFIABLE \n  ~ "      */
    send_result_to_parent(output_to_parent);    

    error = pclose(cnf_file);
    CHECK_ERROR_NEGATIVE(error, "pclose");
}

void send_result_to_parent(char output_to_parent[]) {
    int i;
    // replace any new-line with a space character, so that it is easier to follow the output
    for(i=0; output_to_parent[i] != '\0'; i++) {
        if(output_to_parent[i] == '\n')
            output_to_parent[i] = ' ';
    }
    printf("%s", output_to_parent);
}
