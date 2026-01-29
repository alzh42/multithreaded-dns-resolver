#include "multi-lookup.h"
#include "util.h"
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void *requester_routine(void *arg) {
  // time start
  struct timeval start_time, stop_time;
  gettimeofday(&start_time, NULL);

  requester_struct *requester = arg; // specify the type of arg to be requester thread passed in

  // internal counter for each thread of how many files its serviced
  int files_serviced = 0;
  char *filename = malloc(MAX_NAME_LENGTH * (sizeof(char))); 
  char *hostname = malloc(MAX_NAME_LENGTH * (sizeof(char))); 

  while(1) {
    // obtain file lock for getting filename
    pthread_mutex_lock(requester->input_file_mutex);

    // if all files have been processed, unlock and break out to logging/printing
    if (*requester->file_counter >= requester->num_input_files) {
      pthread_mutex_unlock(requester->input_file_mutex);
      break; 
    }

    strcpy(filename, requester->input_files[*(requester->file_counter)]);
    *(requester->file_counter) += 1;
    pthread_mutex_unlock(requester->input_file_mutex);

    // since we have properly assigned a file to this thread that no other threads will touch
    // we can perform the rest of our file operations safely without that lock
    FILE *file_read = fopen(filename, "r");
    if(file_read == NULL) {
      pthread_mutex_lock(requester->print_mutex);
      fprintf(stderr,"invalid file: %s could not be serviced.\n", filename);
      pthread_mutex_unlock(requester->print_mutex);
      continue;
    }

    files_serviced += 1;
    FILE *log_file = fopen(requester->requester_file, "a");
    while(fgets(hostname, MAX_NAME_LENGTH, file_read)) {
      // remove the newline from the hostname
      size_t length = strlen(hostname);

      if(hostname[length - 1] == '\n') {
        hostname[length - 1] = '\0';
      }
    
      // hostname pointer will get overwritten every loop
      // stdrup allocates unique memory block for string and returns pointer
      // every string will have its own pointer stored in the array instead of shared one to hostname
      array_put(requester->shared_array, strdup(hostname));

      // log the hostname in the requester log file, thread safe
      pthread_mutex_lock(requester->requester_log_mutex);
      fprintf(log_file, "%s\n", hostname);
      pthread_mutex_unlock(requester->requester_log_mutex);
    }

    fclose(log_file);
    fclose(file_read);
  }

  // obtain print lock to print
  gettimeofday(&stop_time, NULL);
  long second_difference = stop_time.tv_sec - start_time.tv_sec;
  long microsecond_difference = stop_time.tv_usec - start_time.tv_usec;

  if(microsecond_difference < 0){
    second_difference -= 1;
    microsecond_difference += 1000000; // add the taken second in microseconds to fix negative diff
  }

  pthread_mutex_lock(requester->print_mutex);
  printf("thread %lu serviced %d files in %ld.%ld seconds\n", pthread_self(), files_serviced, second_difference, microsecond_difference);
  pthread_mutex_unlock(requester->print_mutex);

  free(filename);
  free(hostname);
  return NULL;
}

void *resolver_routine(void *arg) {
    // time start
    struct timeval start_time, stop_time;
    gettimeofday(&start_time, NULL);

    resolver_struct *resolver = arg; // specify the type of arg to be resolver thread passed in

    int hostnames_resolved = 0;
    char *ip_address = malloc(MAX_IP_LENGTH * (sizeof(char)));
    FILE *log_file = fopen(resolver->resolver_file, "a");
    while(1) {
      char *hostname;
      array_get(resolver->shared_array, &hostname);

      // if the resolver finds a poison pill, conclude this thread.
      if(strcmp(hostname, POISON_PILL) == 0) {
        break;
      }

      hostnames_resolved += 1;
      if (dnslookup(hostname, ip_address, MAX_IP_LENGTH) == UTIL_SUCCESS) {
        pthread_mutex_lock(resolver->resolver_log_mutex);
        fprintf(log_file, "%s, %s\n", hostname, ip_address);
        pthread_mutex_unlock(resolver->resolver_log_mutex);
      } else {
        pthread_mutex_lock(resolver->resolver_log_mutex);
        fprintf(log_file, "%s, %s\n", hostname, "NOT_RESOLVED");
        pthread_mutex_unlock(resolver->resolver_log_mutex);
      }

      free(hostname); // free the pointer that was allocated by strdup and passed in by requester
    }

    fclose(log_file);


    gettimeofday(&stop_time, NULL);
    long second_difference = stop_time.tv_sec - start_time.tv_sec;
    long microsecond_difference = stop_time.tv_usec - start_time.tv_usec;

    if(microsecond_difference < 0){
        second_difference -= 1;
        microsecond_difference += 1000000; // add the taken second in microseconds to fix negative diff
    }

    pthread_mutex_lock(resolver->print_mutex);
    printf("thread %lu resolved %d hostnames in %ld.%ld seconds\n", pthread_self(), hostnames_resolved, second_difference, microsecond_difference);
    pthread_mutex_unlock(resolver->print_mutex);
    
    free(ip_address);
    return NULL;
}

int main(int argc, char **argv) {
  // start time
  struct timeval start_time, stop_time;
  gettimeofday(&start_time, NULL);

  if(argc < 6) { 
    fprintf(stderr, "Missing program arguments: multi-lookup <# requester> <# resolver> <requester log> <resolver log> [ <data file> ... ]\n");
    exit(1); // non-zero status, failure
  }

  int num_input_file = argc - 5;
  int num_requesters = atoi(argv[1]);
  int num_resolvers = atoi(argv[2]);
  char *requester_log = argv[3];
  char *resolver_log = argv[4];

  if (num_input_file > 100) {
    fprintf(stderr, "Too many input files were given, maximum of 100.\n");
    exit(1);
  }

  if(num_requesters > 10 || num_resolvers > 10) {
    fprintf(stderr, "Too many requester or resolver threads inputted, maximum of 10 each.\n");
    exit(1);
  }

  if (num_requesters <=  0 || num_resolvers <= 0) {
    fprintf(stderr, "Too little requester or resolver threads inputted, minimum of 1 each.\n");
    exit(1);
  }

  // check the log files. fopen with w will creae a new file if if doesn't exit and overwrite an existing one.
  FILE *requester_file = fopen(requester_log, "w");
  FILE *resolver_file = fopen(resolver_log, "w");

  if((requester_file == NULL) || (resolver_file == NULL)) {
    fprintf(stderr, "Error opening/creating log files.\n");
    exit(1);
  }

  fclose(requester_file);
  fclose(resolver_file);

  // store all input files in an array that requesters can access.
  char *input_file[argc - 5];
  for (int i = 5; i < argc; i++) {
    input_file[i - 5] = argv[i];
  }

  // initialize mutexes
  pthread_mutex_t requester_log_lock;
  pthread_mutex_t resolver_log_lock;
  pthread_mutex_t print_lock;
  pthread_mutex_t file_lock;

  pthread_mutex_init(&requester_log_lock, NULL);
  pthread_mutex_init(&resolver_log_lock, NULL);
  pthread_mutex_init(&print_lock, NULL);
  pthread_mutex_init(&file_lock, NULL);

  array shared_array;

  if (array_init(&shared_array) != 0) {
      printf("Failed to initialize array\n");
      return 1;
  }

  pthread_t requester_threads[num_requesters];
  pthread_t resolver_threads[num_resolvers];

  // initialize requester struct and set initial attributes
  requester_struct requester_struct;
  int *counter = malloc(sizeof(int)); // malloc counter to track which files have been picked up.
  *counter = 0;
  requester_struct.requester_file = requester_log;
  requester_struct.shared_array = &shared_array;
  requester_struct.input_files = input_file;
  requester_struct.file_counter = counter;
  requester_struct.num_input_files = argc - 5;
  requester_struct.requester_log_mutex = &requester_log_lock;
  requester_struct.print_mutex = &print_lock;
  requester_struct.input_file_mutex = &file_lock;

  // initialize resolver struct and set initial attributes
  resolver_struct resolver_struct;
  resolver_struct.resolver_file = resolver_log;
  resolver_struct.shared_array = &shared_array;
  resolver_struct.resolver_log_mutex = &resolver_log_lock;
  resolver_struct.print_mutex = &print_lock;

  // create requester threads
  for (int i = 0; i < num_requesters; i++)
  {
    pthread_create(&requester_threads[i], NULL, requester_routine, &requester_struct);
  }

  // create resolver threads
  for (int i = 0; i < num_resolvers; i++)
  {
    pthread_create(&resolver_threads[i], NULL, resolver_routine, &resolver_struct);
  }

  // wait on all request threads to finish
  for (int i = 0; i < num_requesters; i++)
  {
    pthread_join(requester_threads[i], NULL);
  }

  // after request threads finish, push all resolver poison pills
  for (int i = 0; i < num_resolvers; i++)
  {
    array_put(&shared_array, POISON_PILL);
  }

  // wait for resolvers to join!
  for (int i = 0; i < num_resolvers; i++)
  {
    pthread_join(resolver_threads[i], NULL);
  }
  
  // stop time
  gettimeofday(&stop_time, NULL);

  long second_difference = stop_time.tv_sec - start_time.tv_sec;
  long microsecond_difference = stop_time.tv_usec - start_time.tv_usec;

  if(microsecond_difference < 0){
    second_difference -= 1;
    microsecond_difference += 1000000; // add the taken second in microseconds to fix negative diff
  }

  printf("./multi-lookup: total time is %ld.%ld seconds.\n", second_difference, microsecond_difference);

  pthread_mutex_destroy(&requester_log_lock);
  pthread_mutex_destroy(&resolver_log_lock);
  pthread_mutex_destroy(&print_lock);
  pthread_mutex_destroy(&file_lock);

  free(counter);
  array_free(&shared_array);
  return 0;
}