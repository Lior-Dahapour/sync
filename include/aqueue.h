
#ifndef A_QUEUE_H
#define A_QUEUE_H
#include <stdalign.h>
#include <stdatomic.h>
typedef struct aqueue_node_t aqueue_node_t;
struct aqueue_node_t
{
    _Atomic(aqueue_node_t *) next;
    atomic_size_t refcount;
    void *data;
} __attribute__((aligned(64)));

typedef struct
{
    _Atomic(aqueue_node_t *) head;
    _Atomic(aqueue_node_t *) tail;
} aqueue_t;
void aqueue_init(aqueue_t *list);
void *aqueue_dequeue(aqueue_t *list);
void aqueue_enqueue(aqueue_t *list, void *data);
#endif