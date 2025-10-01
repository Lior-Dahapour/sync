
#include "semaphore.h"
#include <libc.h>
#include "spin.h"
#define SEM_SPIN ((spin_t){.next = 1, .pow = 4, .max = 5})
static inline void init_semaphore_node(semaphore_node_t *node)
{
    parker_init(&node->parker);
    atomic_store_explicit(&node->next, NULL, memory_order_relaxed);
}
int semaphore_init(semaphore_t *sem, int permits)
{
    if (sem == NULL)
        return SEMAPHORE_INIT_FAILED;

    atomic_store_explicit(&sem->permits, permits, memory_order_relaxed);
    atomic_store_explicit(&sem->queue.tail, NULL, memory_order_relaxed);
    atomic_store_explicit(&sem->queue.tail, NULL, memory_order_relaxed);
    sem->capacity = permits;
    return SEMAPHORE_OK;
}

static inline semaphore_node_t *semaphore_dequeue_waiter(semaphore_queue_t *queue)
{
    semaphore_node_t *head;
    semaphore_node_t *next;
    do
    {
        // load the head
        head = atomic_load(&queue->head);
        // if head is null , it mean the queue is empty
        if (!head)
            return NULL;

        // load the next
        next = atomic_load(&head->next);
        // try to exchange head and next
    } while (!atomic_compare_exchange_weak(&queue->head, &head, next));
    // if we exchange the head <-> next
    // if  next is null means the queue is empty
    if (!next)
    {
        // queue is empty
        atomic_store(&queue->tail, NULL);
    }

    atomic_store(&head->next, NULL);
    return head;
}
static inline void semaphore_enequeue_waiter(
    semaphore_queue_t *queue, semaphore_node_t *node)
{

    semaphore_node_t *tail;
    semaphore_node_t *next;
    while (1)
    {

        tail = atomic_load(&queue->tail);
        if (tail == NULL)
        {
            // try to set head to node if empty
            semaphore_node_t *expected = NULL;
            if (atomic_compare_exchange_weak(&queue->head, &expected, node))
            {
                // head is set successfully; also set tail
                atomic_store(&queue->tail, node);
                return;
            }
            // head was changed by another thread, retry
            continue;
        }
        next = atomic_load(&tail->next);
        // check if tail hasn't been changed
        if (tail == atomic_load(&queue->tail))
        {
            // only add if next == NULL
            if (next == NULL)
            {
                // try cas the tail next and node
                // failed if other thread already stored a new node in tail->next
                if (atomic_compare_exchange_weak(&tail->next, &next, node))
                {

                    // even if the cas failed its okay,
                    // its just mean this node is not the tail
                    // we already know the tail->next = node
                    // some other thread already stored into queue->tail
                    atomic_compare_exchange_weak(&queue->tail, &tail, node);
                    return;
                }
            }
            else
            {
                atomic_compare_exchange_weak(&queue->tail, &tail, next);
            }
        }
    }
}
int semaphore_acquire(semaphore_t *sem)
{
    size_t old_permits = atomic_load(&sem->permits);
    spin_t spin = SEM_SPIN;
    while (old_permits > 0)
    {
        if (atomic_compare_exchange_weak(&sem->permits, &old_permits, old_permits - 1))
        {
            return SEMAPHORE_OK;
        }
        else
        {
            spin_next(&spin);
            atomic_store(&sem->permits, old_permits);
            continue;
        }
    }
    return SEMAPHORE_EMPTY;
}

int semaphore_release(semaphore_t *sem)
{

    semaphore_node_t *waiter = semaphore_dequeue_waiter(&sem->queue);
    if (waiter != NULL)
    {
        //  wake the thread
        unpark(&waiter->parker);
        return SEMAPHORE_OK_WOKE;
    }
    else
    {
        atomic_fetch_add(&sem->permits, 1);

        return SEMAPHORE_OK;
    }
}

int semaphore_acquire_block(semaphore_t *sem)
{
    if (semaphore_acquire(sem) == SEMAPHORE_OK)
    {
        return SEMAPHORE_OK;
    }
    semaphore_node_t *node = malloc(sizeof(semaphore_node_t));
    init_semaphore_node(node);
    semaphore_enequeue_waiter(&sem->queue, node);
    park(&node->parker);
    // when we unpark this thread , we don't increase the permits
    // becuase this thread ״immediately״ acquired it
    // that way we know there wasn't any thread who fetch it before the unpark
    parker_destroy(&node->parker);
    free(node);
    return SEMAPHORE_OK_WOKE;
}
