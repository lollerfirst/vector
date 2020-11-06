/** -- Busy Wait Implementation -- **/
#pragma once

#include <unistd.h>
#include <stdint.h>

typedef volatile uint8_t __mutex_semaphore;

static inline void mutex_init(__mutex_semaphore* __mutex){
    __atomic_store_n(__mutex, 0, __ATOMIC_RELAXED);
}

static inline void mutex_wait(__mutex_semaphore* __mutex) {
    uint8_t __expected = 0;
    while(!__atomic_compare_exchange_n(__mutex, &__expected, 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED));
}

static inline void mutex_signal(__mutex_semaphore* __mutex) {
    __atomic_store_n(__mutex, 0, __ATOMIC_RELEASE);
}