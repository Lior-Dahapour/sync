#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdatomic.h>
#include <pthread.h>
#include "parker.h"
#include "aqueue.h"

/**
 * @file semaphore.h
 * @brief A simple counting semaphore implementation supporting both
 *        blocking and non-blocking acquisition.
 *
 * This semaphore manages a fixed number of "permits" that control
 * concurrent access to a shared resource. Threads may:
 *
 *   - Acquire one or more permits (blocking or non-blocking).
 *   - Release permits back, potentially waking blocked waiters.
 *
 *
 * Internally, it uses a FIFO queue for fair acquiring.
 */

/// Forward declarations
typedef struct semaphore_waiter semaphore_waiter_t;
typedef struct semaphore_node semaphore_node_t;

/**
 * @brief Counting semaphore structure.
 *
 * Fields:
 *
 *  - `permits`   : Atomic counter of available permits.
 *
 *  - `capacity`  : Maximum number of permits allowed.
 *
 *  - `head/tail` : Linked list of waiting threads.
 *
 *  - `queue_mutex`: Protects waiter queue operations.
 */
typedef struct
{
    /// @brief Maximum number of permits the semaphore can hold.
    size_t capacity;

    /// @brief Head of the waiter queue.
    semaphore_node_t *head;

    /// @brief Tail of the waiter queue.
    semaphore_node_t *tail;

    /// @brief Mutex guarding access to the waiter queue.
    pthread_mutex_t queue_mutex;

    /// @brief Current number of available permits.
    atomic_size_t permits;

} semaphore_t;

/**
 * @brief Result codes returned by semaphore operations.
 */
typedef enum
{
    /// @brief Operation succeeded.
    SEMAPHORE_OK = 0,

    /// @brief No permits are currently available.
    SEMAPHORE_EMPTY = -1,

    /// @brief Attempted to release more permits than capacity allows
    SEMAPHORE_FULL = -2,

    /// @brief Initialization failed.
    SEMAPHORE_INIT_FAILED = -3,

    /// @brief Semaphore was closed or destroyed while waiting.
    SEMAPHORE_CLOSED = -4,

    /// @brief Not enough permits available for a multi-permit request.
    SEMAPHORE_NOT_ENOUGH = -5,

} SEMAPHORE_RESULT;

/**
 * @brief Initialize a semaphore.
 *
 * @param sem Pointer to the semaphore to initialize.
 * @param init_permits Initial number of available permits.
 * @return SEMAPHORE_OK on success, or SEMAPHORE_INIT_FAILED on error.
 *
 * @note `init_permits` must be <= capacity. Once initialized, the semaphore
 *       may be used concurrently by multiple threads.
 */
int semaphore_init(semaphore_t *sem, size_t init_permits);

/**
 * @brief Attempt to acquire multiple permits without blocking.
 *
 * @param sem Pointer to the semaphore.
 * @param count Number of permits requested.
 * @return SEMAPHORE_OK if successful,
 *         SEMAPHORE_NOT_ENOUGH if insufficient permits,
 *         or SEMAPHORE_CLOSED if the semaphore was destroyed.
 */
int semaphore_acquire_many(semaphore_t *sem, size_t count);

/**
 * @brief Acquire multiple permits, blocking if necessary.
 *
 * @param sem Pointer to the semaphore.
 * @param count Number of permits to acquire.
 * @return SEMAPHORE_OK on success,
 *         SEMAPHORE_CLOSED if the semaphore was destroyed.
 *
 * @note This function may park the calling thread using a per-thread
 *       parker until permits become available.
 */
int semaphore_acquire_many_block(semaphore_t *sem, size_t count);

/**
 * @brief Release multiple permits and wake waiters as needed.
 *
 * @param sem Pointer to the semaphore.
 * @param count Number of permits to release.
 * @return SEMAPHORE_OK on success or SEMAPHORE_FULL if over-released.
 */
int semaphore_release_many(semaphore_t *sem, size_t count);

/**
 * @brief Attempt to acquire a single permit without blocking.
 *
 * @param sem Pointer to the semaphore.
 * @return SEMAPHORE_OK if acquired, SEMAPHORE_EMPTY otherwise.
 */
static inline int semaphore_acquire(semaphore_t *sem)
{
    return semaphore_acquire_many(sem, 1);
}

/**
 * @brief Release a single permit back to the semaphore.
 *
 * If any thread is waiting, one may be woken.
 *
 * @param sem Pointer to the semaphore.
 */
static inline void semaphore_release(semaphore_t *sem)
{
    semaphore_release_many(sem, 1);
}

/**
 * @brief Acquire a single permit, blocking if necessary.
 *
 * @param sem Pointer to the semaphore.
 * @return SEMAPHORE_OK if acquired successfully,
 *         or SEMAPHORE_CLOSED if destroyed while waiting.
 */
static inline int semaphore_acquire_block(semaphore_t *sem)
{
    return semaphore_acquire_many_block(sem, 1);
}

/**
 * @brief Destroy a semaphore.
 *
 * Performs the following:
 *
 *  1. Marks the semaphore as closed (prevents future acquisitions).
 *
 *  2. Wakes all threads currently waiting.
 *
 *  3. Releases all internal resources.
 *
 * @warning After calling this, `sem` must not be used by any thread.
 *          Waiters woken by this function should detect closure the return SEMAPHORE_CLOSED.
 */
void semaphore_destroy(semaphore_t *sem);

#endif /* SEMAPHORE_H */
