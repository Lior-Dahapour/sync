#ifndef PARKER
#define PARKER

#include <pthread.h>
#include <stdatomic.h>
typedef struct
{
    atomic_int state;
    pthread_cond_t condvar;
    pthread_mutex_t mutex;

} parker_t;
int parker_init(parker_t *parker);
int park(parker_t *parker);
int unpark(parker_t *parker);
void parker_destroy(parker_t *parker);

#endif