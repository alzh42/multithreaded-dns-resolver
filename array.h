#ifndef ARRAY_H
#define ARRAY_H
#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>

#define ARRAY_SIZE 8
#define MAX_NAME_LENGTH 20

typedef struct {
    char **buffer;
    int head;
    int tail;
    sem_t csem; 
    sem_t psem; 
    pthread_mutex_t mutex;
} array;

int array_init(array *s);  // initialize the array
int array_put (array *s, char *hostname);  // place element into the array, block when full
int array_get (array *s, char **hostname);  // remove element from the array, block when empty
void array_free(array *s);  // free the array's resources

#endif 