#ifndef PARKER_H
#define PARKER_H

#include <pthread.h>
#include <stdatomic.h>

typedef struct
{
    atomic_int state;
    pthread_cond_t condvar;
    pthread_mutex_t mutex;

} parker_t;
typedef struct
{
    atomic_int state;
    pthread_cond_t condvar;

} waiter_t;
static inline void waiter_init(waiter_t *waiter)
{
    atomic_store(&waiter->state, 0);
    pthread_cond_init(&waiter->condvar, NULL);
}
static inline void waiter_wait(waiter_t *waiter, pthread_mutex_t *mutex)
{
    pthread_mutex_lock(mutex);
    while (atomic_load(&waiter->state) == 0)
    {
        pthread_cond_wait(&waiter->condvar, mutex);
    }
    atomic_store(&waiter->state, 0);
    pthread_mutex_unlock(mutex);
}
static inline void waiter_wake(waiter_t *waiter, pthread_mutex_t *mutex)
{
    pthread_mutex_lock(mutex);
    atomic_store(&waiter->state, 1);
    pthread_cond_signal(&waiter->condvar);
    pthread_mutex_unlock(mutex);
}
static inline void waiter_destroy(waiter_t *waiter)
{
    pthread_cond_destroy(&waiter->condvar);
}
static inline void parker_init(parker_t *parker)
{
    atomic_store(&parker->state, 0);
    pthread_mutex_init(&parker->mutex, NULL);
    pthread_cond_init(&parker->condvar, NULL);
}

static inline void park(parker_t *parker)
{
    pthread_mutex_lock(&parker->mutex);
    while (atomic_load(&parker->state) == 0)
    {
        pthread_cond_wait(&parker->condvar, &parker->mutex);
    }
    atomic_store(&parker->state, 0);
    pthread_mutex_unlock(&parker->mutex);
}

static inline void unpark(parker_t *parker)
{
    pthread_mutex_lock(&parker->mutex);
    atomic_store(&parker->state, 1);
    pthread_cond_signal(&parker->condvar);
    pthread_mutex_unlock(&parker->mutex);
}
static inline void parker_destroy(parker_t *parker)
{
    pthread_mutex_destroy(&parker->mutex);
    pthread_cond_destroy(&parker->condvar);
}

#endif