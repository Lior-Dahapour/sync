#include "aqueue.h"
#include <stdatomic.h>
#include <libc.h>
// -- this impl is based on Michael Scott Queue --
void aqueue_init(aqueue_t *queue)
{
    aqueue_node_t *dummy = malloc(sizeof(aqueue_node_t));
    dummy->data = NULL;
    atomic_store(&dummy->refcount, 2); // 1 for head, 1 for tail
    atomic_store(&dummy->next, NULL);
    atomic_store(&queue->head, dummy);
    atomic_store(&queue->tail, dummy);
}
void *aqueue_dequeue(aqueue_t *queue)
{
    while (1)
    {
        aqueue_node_t *head = atomic_load(&queue->head);
        aqueue_node_t *next = atomic_load(&head->next);
        // if the next is null the queue is empty
        // we don't count the dummy node
        if (!next)
            return NULL;

        void *data = next->data;

        // lets try to move the head forward
        if (atomic_compare_exchange_weak(&queue->head, &head, next))
        {

            // new head gains a reference for the queue
            atomic_fetch_add(&next->refcount, 1);
            // decrement old head's refcount
            if (atomic_fetch_sub(&head->refcount, 1) == 1)
            {
                printf("free \n");
                free(head);
            }

            return data;
        }
    }
}

void aqueue_enqueue(aqueue_t *queue, void *data)
{

    // set new node
    aqueue_node_t *node = malloc(sizeof(aqueue_node_t));
    node->data = data;
    atomic_init(&node->next, NULL);
    atomic_init(&node->refcount, 1);
    while (1)
    {
        // load tail and tail next
        aqueue_node_t *tail = atomic_load(&queue->tail);
        aqueue_node_t *next = atomic_load(&tail->next);

        // check if the tail is the last node, or next is not NULL
        if (!next)
        {
            // the tail is the last node
            // lets try to set the tail next to our mode, if we failed , lets retry.
            if (atomic_compare_exchange_weak(&tail->next, &tail, node))
            {
                // now the tail own the node too
                atomic_fetch_add(&node->refcount, 1);
                // if we successfully stored the next in the tail
                atomic_compare_exchange_strong(&queue->tail, &tail, node);
                // note we use strong here and we don't care about the result, because
                // the only way we fail here, is by knowning other thread help us
                // change the tail to our new node or the tail pointer has been pushed even further
                return;
            }
        }
        else
        {

            // if next is not NULL
            // that mean a thread stored its node in next ,
            // lets try help it and advance the tail into the node

            // NOTE : we still need to enqueue our node!, we failed but we trying to help other thread.
            atomic_compare_exchange_weak(&queue->tail, &tail, tail->next);
        }
    }
}
