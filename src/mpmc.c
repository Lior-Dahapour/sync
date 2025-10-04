
#include <stdatomic.h>
#include <stdlib.h>
#include <libc.h>
#include "mpmc.h"
#include "parker.h"
#include "spin.h"
#define MPMC_SPIN ((spin_t){.next = 1, .pow = 2, .max = 6})
static inline mpmc_cell_t *mpmc_get_cell(mpmc_t *queue, size_t i)
{

    size_t cell_size = sizeof(mpmc_cell_t) + queue->item_size;
    return (mpmc_cell_t *)(queue->buffer + i * cell_size);
}

int mpmc_init(mpmc_t *queue, int capacity, int item_size)
{
    queue->buffer = malloc(capacity * (sizeof(mpmc_cell_t) + item_size));

    if (queue->buffer == NULL)
    {
        return MPMC_INIT_FAILED;
    }

    queue->capacity = capacity;
    queue->item_size = item_size;
    atomic_store(&queue->head, 0);
    atomic_store(&queue->tail, 0);
    for (size_t i = 0; i < capacity; i++)
    {
        // get the cell
        mpmc_cell_t *cell = mpmc_get_cell(queue, i);
        // init the seq
        atomic_store(&cell->seq, i);
    }
    parker_init(&queue->send_parker);
    parker_init(&queue->recv_parker);

    return MPMC_OK;
}

int mpmc_send(mpmc_t *queue, void *message)
{
    size_t tail;
    size_t seq;
    spin_t spin = MPMC_SPIN;
    while (1)
    {
        tail = atomic_load(&queue->tail);
        mpmc_cell_t *cell = mpmc_get_cell(queue, tail % queue->capacity);
        seq = atomic_load(&cell->seq);
        if (seq == tail)
        {
            if (atomic_compare_exchange_weak(&queue->tail, &tail, tail + 1))
            {
                memcpy(cell->data, message, queue->item_size);
                atomic_store(&cell->seq, tail + 1);
                if (atomic_load(&queue->recv_waiting) > 0)
                {
                    unpark(&queue->recv_parker);
                }

                return MPMC_OK;
            }
            else
            {
                if (spin_next(&spin) == TRUE)
                {
                    // NOTE : this isn't always mean the mpmc is full , but if we retry this many time,
                    // it mean the send is "blocking", which isn't the purpose of this function
                    return MPMC_EMPTY;
                }
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
    spin_t spin = MPMC_SPIN;
    while (1)
    {
        head = atomic_load(&queue->head);
        mpmc_cell_t *cell = mpmc_get_cell(queue, head % queue->capacity);
        seq = atomic_load(&cell->seq);
        if (seq == head + 1)
        {
            if (atomic_compare_exchange_weak(&queue->head, &head, head + 1))
            {
                memcpy(message, cell->data, queue->item_size);
                atomic_store(&cell->seq, head + queue->capacity);
                if (atomic_load(&queue->send_waiting) > 0)
                {
                    unpark(&queue->send_parker);
                    /* code */
                }
                return MPMC_OK;
            }
            else
            {
                if (spin_next(&spin) == TRUE)
                {
                    // NOTE : this isn't always mean the mpmc is empty , but if we retry this many time,
                    // it mean the recv is "blocking", which isn't the purpose of this function
                    return MPMC_EMPTY;
                }
            }
        }
        else
        {

            return MPMC_EMPTY;
        }
    }
}
int mpmc_send_block(mpmc_t *queue, void *message)
{
    while (1)
    {
        if (mpmc_send(queue, message) == MPMC_FULL)
        {
            atomic_fetch_add(&queue->send_waiting, 1);
            park(&queue->send_parker);
            atomic_fetch_sub(&queue->send_waiting, 1);
            continue;
        }
        return MPMC_OK;
    }
}
int mpmc_recv_block(mpmc_t *queue, void *message)
{
    while (1)
    {
        if (mpmc_recv(queue, message) == MPMC_EMPTY)
        {
            atomic_fetch_add(&queue->recv_waiting, 1);
            park(&queue->recv_parker);
            atomic_fetch_sub(&queue->recv_waiting, 1);
            continue;
        }
        return MPMC_OK;
    }
}
void destroy_mpmc(mpmc_t *queue)
{
    free(queue->buffer);

    parker_destroy(&queue->recv_parker);
    parker_destroy(&queue->send_parker);
}