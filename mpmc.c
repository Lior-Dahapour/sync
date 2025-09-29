
#include "stdatomic.h"
#include "stdlib.h"
#include "libc.h"

#include "mpmc.h"

static inline mpmc_cell_t *mpmc_get_cell(mpmc_t *queue, size_t i)
{

    size_t cell_size = sizeof(mpmc_cell_t) + queue->item_size;
    return (mpmc_cell_t *)(queue->buffer + i * cell_size);
}

/// @brief init a new mpmc channel
/// @param capacity the amount if items the channel can hold without recv before its full
/// @param item_size the size of the item the channel expceting
/// @return
int mpmc_init(mpmc_t *queue, int capacity, int item_size)
{
    // alloc the inner channel buffer
    void *inner = malloc(capacity * (item_size + sizeof(atomic_size_t)));

    if (inner == NULL)
    {
        return MPMC_INIT_FAILED;
    }

    queue->capacity = capacity;
    queue->buffer = inner;
    queue->item_size = item_size;
    atomic_store(&queue->head, 0);
    atomic_store(&queue->tail, 0);
    for (size_t i = 0; i < capacity; i++)
    {
        // atomic_store(&queue->buffer[i].stamp, i);
    }

    return 0;
}

/// @brief send a message to the channel \r\n this function is non blocking , if the channel is full
/// @param queue the  channel
/// @param message the message to the channel, this copy the message to the channel buffer , if the message live in the heap you must free the memory manully

int mpmc_send(mpmc_t *queue, void *message)
{
    size_t tail;
    size_t seq;
    while (1)
    {
        tail = atomic_load(&queue->tail);
        mpmc_cell_t *cell = mpmc_get_cell(queue, tail % queue->capacity);
        seq = atomic_load(&cell->seq);
        if (seq == tail)
        {
            if (atomic_compare_exchange_weak(&queue->tail, &tail, tail + 1))
            {
                memcpy(&cell->data, message, queue->item_size);
                atomic_store(&cell->seq, tail + 1);
                return MPMC_OK;
            }
            else
            {
                // CAS failed
                continue;
            }
        }
        else
        {

            return MPMC_FULL;
        }
    }
};

int mpmc_recv(mpmc_t *queue, void *message)
{
    size_t head;
    size_t seq;
    while (1)
    {
        head = atomic_load(&queue->head);
        mpmc_cell_t *cell = mpmc_get_cell(queue, head % queue->capacity);
        seq = atomic_load(&cell->seq);
        if (seq == head + 1)
        {
            if (atomic_compare_exchange_weak(&queue->head, &head, head + 1))
            {
                memcpy(message, &cell->data, queue->item_size);
                atomic_store(&cell->seq, head + 1);
                return MPMC_OK;
            }
            else
            {
                // CAS failed
                continue;
            }
        }
        else
        {

            return MPMC_EMPTY;
        }
    }
}
