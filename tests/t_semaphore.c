#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "semaphore.h"
#include "unistd.h"
#define NUM_THREADS 32
#define INITIAL_PERMITS 7

semaphore_t *sem;

void *worker(void *arg)
{
    int id = *(int *)arg;

    printf("Thread %d: trying to acquire semaphore...\n", id);
    if (semaphore_acquire_many_block(sem, 2) == SEMAPHORE_CLOSED)
    {
        printf("Thread %d: waking up by semaphore close\n", id);
    }
    else
    {
        printf("Thread %d: acquired semaphore!\n", id);
        usleep(100000); // 100ms

        printf("Thread %d: releasing semaphore\n", id);
        semaphore_release_many(sem, 2);
    }

    return NULL;
}

int main()
{
    sem = malloc(sizeof(semaphore_t));

    pthread_t threads[NUM_THREADS];
    int ids[NUM_THREADS];

    // initialize semaphore with INITIAL_PERMITS
    if (semaphore_init(sem, INITIAL_PERMITS) != SEMAPHORE_OK)
    {

        fprintf(stderr, "Failed to initialize semaphore\n");
        return 1;
    }

    // create threads
    for (int i = 0; i < NUM_THREADS; i++)
    {
        ids[i] = i + 1;
        if (pthread_create(&threads[i], NULL, worker, &ids[i]) != 0)
        {
            fprintf(stderr, "Failed to create thread %d\n", i);
            return 1;
        }
    }

    //  usleep(350000);
    //  semaphore_destroy(sem);
    // wait for threads
    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    printf("All threads finished.\n");
    return 0;
}
