#ifndef SPIN_H
#define SPIN_H

typedef struct
{
    int next;
    int pow;
    int max;
} spin_t;

// similiar to rust hint spin
#if defined(_WIN32) || defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#define CPU_HINT_LOOP() _mm_pause()
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_acle.h>
#define CPU_HINT_LOOP() __isb(0xF)
#elif defined(__arm__)
#include <arm_acle.h>
#define CPU_HINT_LOOP() __yield()
#elif defined(__riscv)
#define CPU_HINT_LOOP() __asm__ __volatile__("pause" ::: "memory")
#else
#define CPU_HINT_LOOP() __asm__ __volatile__("" ::: "memory")
#endif

static inline void spin_next(spin_t *spin)
{
    if (spin->next == spin->max)
    {
        spin->next = spin->max;
    }
    for (int i = 0; i < (spin->next ^ spin->pow); i++)
    {
        CPU_HINT_LOOP();
    }
    spin->next += 1;
}

#endif