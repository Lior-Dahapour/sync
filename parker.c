#include "parker.h"
#include "pthread.h"

int parker_init(parker_t *parker)
{
    atomic_store(&parker->state, 0);
    pthread_mutex_init(&parker->mutex, NULL);
    pthread_cond_init(&parker->condvar, NULL);
    return 0;
}

int park(parker_t *parker)
{
    pthread_mutex_lock(&parker->mutex);
    while (atomic_load(&parker->state) == 0)
    {
        pthread_cond_wait(&parker->condvar, &parker->mutex);
    }
    atomic_store(&parker->state, 0);
    pthread_mutex_unlock(&parker->mutex);
    return 0;
}

int unpark(parker_t *parker)
{
    pthread_mutex_lock(&parker->mutex);
    atomic_store(&parker->state, 1);
    pthread_cond_signal(&parker->condvar);
    pthread_mutex_unlock(&parker->mutex);
    return 0;
}