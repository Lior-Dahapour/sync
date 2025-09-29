#ifndef MPMC
#define MPMC

#include <stdatomic.h>
#include "parker.h"
typedef enum
{
    MPMC_OK = 1,
    MPMC_FULL = -1,
    MPMC_EMPTY = -2,
    MPMC_INIT_FAILED = -3,

} MPMC_RESULT;
typedef struct
{
    atomic_size_t seq;    // sequence number
    unsigned char data[]; // inline storage for item
} mpmc_cell_t;
typedef struct
{
    mpmc_cell_t *buffer;
    atomic_size_t tail;
    atomic_size_t head;
    size_t item_size;
    int capacity;
    parker_t recv_parker;
    parker_t send_parker;
} mpmc_t;
/**
 * @brief Receive a message from the MPMC queue (non-blocking).
 *
 * Copies the next available item from the internal buffer into `message`.
 * If the queue is empty, returns MPMC_EMPTY immediately.
 *
 * @param queue Pointer to the MPMC queue.
 * @param message Pointer to a buffer where the received item will be copied.
 * @return MPMC_OK on success, MPMC_EMPTY if the queue is empty.
 */
int mpmc_recv(mpmc_t *queue, void *message);
/**
 * @brief Send a message to the MPMC queue (non-blocking).
 *
 * Copies the message into the internal buffer of the queue.
 * The queue stores the data **inline**, so after this call, the user is free
 * to reuse or free the memory pointed to by `message` if it was dynamically allocated.
 *
 * @note The queue does **not** take ownership of heap-allocated memory.
 *       You must free it yourself if needed after sending.
 *
 * @param queue Pointer to the MPMC queue.
 * @param message Pointer to the message data to send.
 * @return MPMC_OK on success, MPMC_FULL if the queue is full.
 */
int mpmc_send(mpmc_t *queue, void *message);

/**
 * @brief Initialize a bounded MPMC queue.
 *
 * Allocates internal buffer to hold `capacity` items, each of size `item_size`.
 * Initializes the head and tail counters for lock-free operations.
 *
 * @param queue Pointer to an already allocated mpmc_t struct.
 * @param capacity Maximum number of items the queue can hold.
 * @param item_size Size in bytes of each item.
 * @return MPMC_OK on success, MPMC_INIT_FAILED on allocation failure.
 */
int mpmc_init(mpmc_t *queue, int capacity, int item_size);
int mpmc_send_block(mpmc_t *queue, void *message);
int mpmc_recv_block(mpmc_t *queue, void *message);
void destroy_mpmc(mpmc_t *queue);
#endif