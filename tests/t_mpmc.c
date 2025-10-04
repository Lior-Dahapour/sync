#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "mpmc.h"

#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 3
#define ITEMS_PER_PRODUCER 5
#define QUEUE_CAPACITY 5

mpmc_t queue;

// Producer thread function
void *producer(void *arg)
{

    int id = *(int *)arg;
    for (int i = 0; i < ITEMS_PER_PRODUCER; i++)
    {
        int item = id * 100 + i;
        mpmc_send_block(&queue, &item);
        printf("Producer %d sent: %d\n", id, item);
        usleep(100000); // simulate work
    }
    return NULL;
}

// Consumer thread function
void *consumer(void *arg)
{
    int id = *(int *)arg;
    for (int i = 0; i < ITEMS_PER_PRODUCER; i++)
    {
        int item;
        mpmc_recv_block(&queue, &item);
        printf("Consumer %d received: %d\n", id, item);
        usleep(150000); // simulate work
    }
    return NULL;
}

int main()
{
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
    int ids[NUM_PRODUCERS > NUM_CONSUMERS ? NUM_PRODUCERS : NUM_CONSUMERS];

    // Initialize the queue
    if (mpmc_init(&queue, QUEUE_CAPACITY, sizeof(int)) != MPMC_OK)
    {
        fprintf(stderr, "Failed to initialize MPMC queue\n");
        return 1;
    }

    // Start producer threads
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        ids[i] = i + 1;
        pthread_create(&producers[i], NULL, producer, &ids[i]);
    }

    // Start consumer threads
    for (int i = 0; i < NUM_CONSUMERS; i++)
    {
        ids[i] = i + 1;
        pthread_create(&consumers[i], NULL, consumer, &ids[i]);
    }

    // Wait for all threads to finish
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        pthread_join(producers[i], NULL);
    }
    for (int i = 0; i < NUM_CONSUMERS; i++)
    {
        pthread_join(consumers[i], NULL);
    }

    destroy_mpmc(&queue);
    printf("All producers and consumers finished.\n");
    return 0;
}
