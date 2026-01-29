#include "array.h"

int array_init(array *s) { // initialize the array
    s->buffer = malloc(ARRAY_SIZE * sizeof(char *)); // just malloc an array of pointers.

    if(s->buffer == NULL) {
        return -1;
    }

    s->head = 0;
    s->tail = 0;

    // https://man7.org/linux/man-pages/man3/sem_init.3.html
    sem_init(&s->csem, 0, 0);
    sem_init(&s->psem, 0, ARRAY_SIZE);

    pthread_mutex_init(&s->mutex, NULL);
    return 0;
} 

int array_put (array *s, char *hostname) {  // place element into the array, block when full
    sem_wait(&s->psem); // check producer semaphore value
    pthread_mutex_lock(&s->mutex); // lock before the critical section

    s->buffer[s->tail] = hostname;
    s->tail = (s->tail + 1) % ARRAY_SIZE;

    pthread_mutex_unlock(&s->mutex);
    sem_post(&s->csem);
    return 0;
}

int array_get (array *s, char **hostname) { // remove element from the array, block when empty
    sem_wait(&s->csem); // check the consumer semaphore value
    pthread_mutex_lock(&s->mutex); // lock before the critical section

    // remove from the array
    *hostname = s->buffer[s->head];
    s->head = (s->head + 1) % ARRAY_SIZE;

    pthread_mutex_unlock(&s->mutex);
    sem_post(&s->psem);
    return 0;
}  

void array_free(array *s) { // free the array's resources
    // free array memory
    if(s->buffer != NULL) {
        free(s->buffer);
    }

    sem_destroy(&s->csem);
    sem_destroy(&s->psem);

    pthread_mutex_destroy(&s->mutex);
}