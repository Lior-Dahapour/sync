
#include "semaphore.h"
#include <libc.h>
#include "spin.h"
#include "aqueue.h"
#define SEM_SPIN ((spin_t){.next = 1, .pow = 4, .max = 7})
#define CLOSE_BIT ((size_t)1 << (sizeof(size_t) * (8 - 1)))
#define MAX_PERMITS (SIZE_MAX ^ CLOSE_BIT)

struct semaphore_waiter
{
    parker_t parker;
    size_t wants;
};

struct semaphore_node
{
    semaphore_waiter_t *waiter;
    struct semaphore_node *next;
};
static inline int is_semaphore_closed(semaphore_t *sem)
{
    return (atomic_load(&sem->permits) & CLOSE_BIT) != 0;
}
static inline semaphore_waiter_t *semaphore_dequeue_locked(semaphore_t *sem)
{
    if (sem->head == NULL)
    {
        // if queue is empty
        return NULL;
    }
    semaphore_node_t *removed = sem->head;
    semaphore_waiter_t *result = sem->head->waiter;
    // advance the head
    sem->head = sem->head->next;
    // if the queue is now empty
    if (sem->head == NULL)
        sem->tail = NULL;

    free(removed);
    return result;
}
static inline int semaphore_enqueue(semaphore_t *sem, semaphore_waiter_t *waiter)
{
    semaphore_node_t *node = malloc(sizeof(semaphore_node_t));
    if (!node)
        return FALSE;

    node->waiter = waiter;
    node->next = NULL;

    pthread_mutex_lock(&sem->queue_mutex);

    // case 1: semaphore already closed
    if (is_semaphore_closed(sem))
    {
        pthread_mutex_unlock(&sem->queue_mutex);
        waiter->wants |= CLOSE_BIT;
        free(node);
        return FALSE;
    }

    // case 2 : maybe enough permits were just released before we locked
    if (semaphore_acquire_many(sem, waiter->wants) == SEMAPHORE_OK)
    {
        pthread_mutex_unlock(&sem->queue_mutex);
        free(node);
        return FALSE; // acquired immediately, no need to park
    }

    // case 3: actually enqueue
    if (sem->tail == NULL)
    {
        sem->head = sem->tail = node;
    }
    else
    {
        sem->tail->next = node;
        sem->tail = node;
    }

    pthread_mutex_unlock(&sem->queue_mutex);

    return TRUE;
}

static inline void semaphore_release_permits(semaphore_t *sem, size_t *released)
{
    pthread_mutex_lock(&sem->queue_mutex);
    while (*released > 0)
    {
        if (sem->head == NULL)
        {
            break;
        }
        semaphore_node_t *removed = sem->head;
        semaphore_waiter_t *waiter = sem->head->waiter;
        // advance the head
        sem->head = sem->head->next;
        if (waiter->wants > *released)
        {
            waiter->wants -= *released;
            *released = 0;
        }
        else
        {
            *released -= removed->waiter->wants;
            unpark(&removed->waiter->parker);
            free(removed);
        }

        // if the queue is now empty
        if (sem->head == NULL)
        {
            sem->tail = NULL;
            break;
        }
    }
    atomic_fetch_add_explicit(&sem->permits, *released, memory_order_release);
    pthread_mutex_unlock(&sem->queue_mutex);
}

int semaphore_init(semaphore_t *sem, size_t permits)
{
    if (sem == NULL || permits > MAX_PERMITS)
        return SEMAPHORE_INIT_FAILED;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    if (pthread_mutex_init(&mutex, NULL) < 0)
        return SEMAPHORE_INIT_FAILED;

    atomic_store_explicit(&sem->permits, permits, memory_order_relaxed);
    sem->tail = NULL;
    sem->head = NULL;
    sem->queue_mutex = mutex;
    sem->capacity = permits;
    return SEMAPHORE_OK;
}

int semaphore_acquire_many(semaphore_t *sem, size_t permits)
{

    size_t current = atomic_load_explicit(&sem->permits, memory_order_acquire);
    spin_t spin = SEM_SPIN;
    while (1)
    {
        if ((current & CLOSE_BIT) != 0)
        {
            return SEMAPHORE_CLOSED;
        }
        if ((current & ~CLOSE_BIT) < permits)
        {
            return SEMAPHORE_NOT_ENOUGH;
        }
        size_t new = current - permits;

        if (atomic_compare_exchange_weak_explicit(&sem->permits, &current, new, memory_order_acq_rel, memory_order_acquire))
        {
            return SEMAPHORE_OK;
        }
        spin_next(&spin);
    }
}
int semaphore_acquire_many_block(semaphore_t *sem, size_t permits)
{
    int result = semaphore_acquire_many(sem, permits);
    if (result == SEMAPHORE_NOT_ENOUGH)
    {
        semaphore_waiter_t *waiter = malloc(sizeof(semaphore_waiter_t));
        parker_init(&waiter->parker);
        waiter->wants = permits;
        if (semaphore_enqueue(sem, waiter))
            park(&waiter->parker);

        parker_destroy(&waiter->parker);
        int result = SEMAPHORE_OK;
        if ((waiter->wants & CLOSE_BIT) != 0)
            result = SEMAPHORE_CLOSED;
        free(waiter);
        return result;
    }
    else
        return result;
}
int semaphore_release_many(semaphore_t *sem, size_t permits)
{

    semaphore_release_permits(sem, &permits);

    return 0;
}
static inline int set_semaphore_closed(semaphore_t *sem)
{
    return (atomic_fetch_or(&sem->permits, CLOSE_BIT) & CLOSE_BIT) != 0;
}

void semaphore_destroy(semaphore_t *sem)
{
    if (!set_semaphore_closed(sem))
    {

        pthread_mutex_lock(&sem->queue_mutex);
        semaphore_waiter_t *node = semaphore_dequeue_locked(sem);
        while (node != NULL)
        {
            // SAFETY -> wants isn't atomic but because of the mutex gaurded queue
            // we know only one thread can remove this node and the thread who "waiter" node is in wating state
            // so only this thread have "mut" acess to the node
            node->wants |= CLOSE_BIT;
            unpark(&node->parker);
            node = semaphore_dequeue_locked(sem);
        }
        pthread_mutex_unlock(&sem->queue_mutex);
    }
}
