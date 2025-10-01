#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdatomic.h>
#include "parker.h"

/// @brief Node representing a single thread waiting on the semaphore
typedef struct semaphore_node_t semaphore_node_t;
struct semaphore_node_t
{
    /// @brief Parker used to park/unpark a thread
    parker_t parker;

    /// @brief Pointer to the next waiting node in the queue
    _Atomic(semaphore_node_t *) next;
};

/// @brief Lock-free queue of waiting threads for the semaphore
typedef struct
{
    /// @brief Head of the waiting queue
    _Atomic(semaphore_node_t *) head;

    /// @brief Tail of the waiting queue
    _Atomic(semaphore_node_t *) tail;
} semaphore_queue_t;

/// @brief A semaphore is a synchronization primitive used to control access
///        to a shared resource by multiple threads. It works like a counter
///        of available "permits":
///
///        - Threads acquire a permit before accessing the resource.
///        - Threads release the permit after finishing.
///
///        Semaphores can be used to limit concurrent access to resources like
///        printers, database connections, or any limited system resource.
///
///        This implementation supports both non-blocking and blocking
///        acquisition of permits.
typedef struct
{
    /// @brief Maximum number of permits available
    size_t capacity;

    /// @brief Queue of waiting threads
    semaphore_queue_t queue;

    /// @brief Current number of available permits
    atomic_size_t permits;

} semaphore_t;

/// @brief Return codes for semaphore operations
typedef enum
{
    /// @brief The operation succeeded without waking any thread
    SEMAPHORE_OK = 0,

    /// @brief A waiting thread was woken and acquired the permit
    SEMAPHORE_OK_WOKE = 1,

    /// @brief There are no permits available
    SEMAPHORE_EMPTY = -1,

    /// @brief The semaphore is already full (used for limited capacity semaphores)
    SEMAPHORE_FULL = -2,

    /// @brief Failed to initialize the semaphore
    SEMAPHORE_INIT_FAILED = -3,

} SEMAPHORE_RESULT;

/// @brief Initialize a semaphore
/// @param sem Pointer to the semaphore to initialize
/// @param init_permits Initial number of available permits
/// @return SEMAPHORE_OK on success, SEMAPHORE_INIT_FAILED on failure
int semaphore_init(semaphore_t *sem, int init_permits);

/// @brief Try to acquire a permit without blocking
/// @param sem Pointer to the semaphore
/// @return SEMAPHORE_OK if a permit was acquired,
///         SEMAPHORE_EMPTY if no permits are available
int semaphore_acquire(semaphore_t *sem);

/// @brief Release a permit back to the semaphore
///        if there are waiting threads, wakes one of them
/// @param sem Pointer to the semaphore
/// @return SEMAPHORE_OK if incremented permits,
///         SEMAPHORE_OK_WOKE if a waiting thread was woken
int semaphore_release(semaphore_t *sem);

/// @brief Acquire a permit, blocking if none are available
///        will park the calling thread until a permit becomes available
/// @param sem Pointer to the semaphore
/// @return SEMAPHORE_OK if the acquired the permit immediately ,
/// SEMAPHORE_OK_WOKE if acquired by a release call
int semaphore_acquire_block(semaphore_t *sem);

#endif
