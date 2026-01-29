#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H
#include "array.h"

// ARRAY_SIZE (8) and MAX_NAME_LENGTH (20) already defined in array.h
#define MAX_INPUT_FILES 100
#define MAX_REQUESTER_THREADS 10
#define MAX_RESOLVER_THREADS 10
#define MAX_IP_LENGTH INET6_ADDRSTRLEN
#define POISON_PILL "STOP"

typedef struct {
    char *requester_file; // file requested hostnames are written into, requester log
    array *shared_array; // shared array hostnames are put into
    char **input_files; // array of input files names
    int *file_counter; // counter tracking which input file to pick up next
    int num_input_files; // total number of input files to service
    pthread_mutex_t *requester_log_mutex; // mutex for requester log
    pthread_mutex_t *input_file_mutex; // mutex for grabbing an input file
    pthread_mutex_t *print_mutex; // mutex for printing to stdout
} requester_struct;

typedef struct {
    char *resolver_file; // file with resolved hostanames and their ip addresses
    array *shared_array; // shared array hostnames are resolved from
    pthread_mutex_t *resolver_log_mutex; // mutex for resolver log
    pthread_mutex_t *print_mutex; // mutex for printing to stdout
} resolver_struct;
#endif