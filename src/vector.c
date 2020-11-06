#define _GNU_SOURCE

#include <stdint.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <memory.h>
#include "/home/lollerfirst/development/async/semaphore.h"

#define __VECTOR_MAX_SIZE 1073741824  // 1 GB max size
#define __VECTOR_POPHEAD_DELAY 48
#define __SWAP_GAMMA 0.30F

#define unlikely(x) __builtin_expect(x, 0)
#define likely(x) __builtin_expect(x, 1)

#define vector_memcheck(__vec) {\
    if(unlikely(__vec->__vector_swap_indices[0] != __vec->__vector_swap_indices[0] && __vec->__vector_swap_indices[0] > 0)){\
    uintptr_t swap_addr1 = (uintptr_t) __vec->__vector_ptr + ((__vec->__vector_swap_indices[0]-1) * __PAGE_SIZE);\
    uintptr_t swap_addr2 = (uintptr_t) __vec->__vector_ptr + ((__vec->__vector_swap_indices[1]-1) * __PAGE_SIZE);\
    posix_madvise((void*)swap_addr1, __PAGE_SIZE, MADV_WILLNEED);\
    posix_madvise((void*)swap_addr2, __PAGE_SIZE, MADV_DONTNEED);\
    }\
}


static uint32_t __PAGE_SIZE;

typedef unsigned __int128 uint128_t;

typedef struct __vector_type{ uint8_t* __vector_ptr; 
                         uint32_t __vector_pages;
                         int32_t __vector_swap_indices[2];
                         uint64_t __vector_size; 
                         uint64_t __vector_element_size; 
                         uint64_t __vector_index; 
                         __mutex_semaphore __vector_sem; 
                         uint16_t __vector_head_popped;
                         } Vector;

void vector_readn(Vector* restrict __vec, void* restrict  __dest, const uint64_t __index, const uint64_t __element_count);
void vector_storen(Vector* restrict __vec, const uint8_t* restrict __src, const uint64_t __index, const uint64_t __element_count);
int vector_init(Vector* __vec, const uint64_t __size, const uint64_t __type_size);
int vector_pushn(Vector* restrict, const void* restrict , const uint64_t __element_count);
int vector_pushf(Vector* restrict __vec, const void* restrict __src);
int vector_popf(Vector* restrict __vec, void* restrict __dest);
void* vector_at(Vector* restrict __vec, const uint64_t __index);
static inline int vector_expand(Vector*);
static inline int vector_shrink(Vector*);
 
int vector_init(Vector* __vec, const uint64_t __size, const uint64_t __type_size){
    memset(__vec, 0x0, sizeof(Vector));
    __PAGE_SIZE = (uint32_t) sysconf(_SC_PAGESIZE);
    
    uint8_t* tmp_ptr;
    if((tmp_ptr = mmap(NULL, ((__size*__type_size) / __PAGE_SIZE) + __PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) == (uint8_t*)-1 || (uint128_t)__size * __type_size >= __VECTOR_MAX_SIZE) return -1;

    __vec->__vector_ptr = tmp_ptr;
    __vec->__vector_pages = (__size*__type_size / __PAGE_SIZE) + 1;
    __vec->__vector_swap_indices[0] = 1;
    __vec->__vector_size = (__size*__type_size / __PAGE_SIZE) + __PAGE_SIZE;
    __vec->__vector_element_size = __type_size;

    posix_madvise((void*) tmp_ptr, (__size / __PAGE_SIZE) + __PAGE_SIZE, POSIX_MADV_SEQUENTIAL);
    return 0;
}


static inline int vector_expand(Vector* __vec){
    uint8_t* tmp_ptr;
    if((tmp_ptr = mremap((void*)__vec->__vector_ptr, __vec->__vector_size, __vec->__vector_size + __PAGE_SIZE, MREMAP_MAYMOVE)) == (void*)-1) return -1;
    
    __vec->__vector_size += __PAGE_SIZE;
    ++__vec->__vector_pages;
    __vec->__vector_ptr = tmp_ptr;

    posix_madvise((void*) tmp_ptr, __vec->__vector_size, POSIX_MADV_SEQUENTIAL);
    return 0;
}

static inline int vector_shrink(Vector* __vec){
    uint8_t* tmp_ptr;

    if((tmp_ptr = mremap((void*)__vec->__vector_ptr, __vec->__vector_size, __vec->__vector_size - __PAGE_SIZE, MREMAP_MAYMOVE)) == (void*)-1){
        return -1;
    }
    __vec->__vector_size -= __PAGE_SIZE;
    --__vec->__vector_pages;
    __vec->__vector_ptr = tmp_ptr;

    posix_madvise((void*) tmp_ptr, __vec->__vector_size, POSIX_MADV_SEQUENTIAL);
    return 0;
}

void vector_readn(Vector* restrict __vec, void* restrict  __dest, const uint64_t __index, const uint64_t __element_count){
    uintptr_t sem = (uintptr_t) &__vec->__vector_sem;
    mutex_wait((__mutex_semaphore*) sem);
    uint64_t element_size = __vec->__vector_element_size;
    uintptr_t tmp_ptr = (uintptr_t) __vec->__vector_ptr + (__index*element_size);
    memcpy(__dest, (void*)tmp_ptr, element_size*__element_count);

    /* calculating exponential moving average of blocks accessed */
    uint32_t accessed_block = (((uint8_t*)tmp_ptr - __vec->__vector_ptr) / __PAGE_SIZE) + 1;
    uint32_t zero_swap_index = __vec->__vector_swap_indices[0];
    __vec->__vector_swap_indices[1] = zero_swap_index; 
    __vec->__vector_swap_indices[0] = (uint32_t)(accessed_block * __SWAP_GAMMA + (zero_swap_index * ((float)1-__SWAP_GAMMA)));
    
    vector_memcheck(__vec);
    mutex_signal((__mutex_semaphore*) sem);
}  


void vector_storen(Vector* restrict __vec, const uint8_t* restrict __src, const uint64_t __index, const uint64_t __element_count) {
    uintptr_t sem = (uintptr_t) &__vec->__vector_sem;
    mutex_wait((__mutex_semaphore*) sem);
    uint64_t element_size = __vec->__vector_element_size;
    uintptr_t tmp_ptr = (uintptr_t) __vec->__vector_ptr + (__index*element_size);
    memcpy((void*)tmp_ptr, __src, element_size*__element_count);

    /* calculating exponential moving average of blocks accessed */
    uint32_t accessed_block = (((uint8_t*)tmp_ptr - __vec->__vector_ptr) / __PAGE_SIZE) + 1;
    uint32_t zero_swap_index = __vec->__vector_swap_indices[0];
    __vec->__vector_swap_indices[1] = zero_swap_index; 
    __vec->__vector_swap_indices[0] = (uint32_t)(accessed_block * __SWAP_GAMMA + (zero_swap_index * ((1.0F-__SWAP_GAMMA))));
    
    vector_memcheck(__vec);
    mutex_signal((__mutex_semaphore*) sem);
}


int vector_pushn(Vector* restrict __vec, const void* restrict __src, const uint64_t __element_count){
    uintptr_t sem = (uintptr_t) &__vec->__vector_sem;
    mutex_wait((__mutex_semaphore*) sem);
    uint64_t element_size = __vec->__vector_element_size;
    uint64_t index = __vec->__vector_index;
    if(unlikely((index+__element_count) * element_size >= __vec->__vector_size)){
        if(vector_expand(__vec) < 0){mutex_signal((__mutex_semaphore*) sem); return -1;}
    }

    
    uintptr_t tmp_ptr = (uintptr_t) __vec->__vector_ptr + (index*element_size);
    memcpy((void*)tmp_ptr, __src, element_size*__element_count);
    __vec->__vector_index += __element_count;

    /* calculating exponential moving average of blocks accessed */
    uint32_t accessed_block = (((uint8_t*)tmp_ptr - __vec->__vector_ptr) / __PAGE_SIZE) + 1;
    uint32_t zero_swap_index = __vec->__vector_swap_indices[0];
    __vec->__vector_swap_indices[1] = zero_swap_index; 
    __vec->__vector_swap_indices[0] = (uint32_t)(accessed_block * __SWAP_GAMMA + (zero_swap_index * ((float)1-__SWAP_GAMMA)));
    
    vector_memcheck(__vec);

    mutex_signal((__mutex_semaphore*) sem);
    return 0;
}

int vector_pushf(Vector* restrict __vec, const void* restrict __src){
    uintptr_t sem = (uintptr_t) &__vec->__vector_sem;
    mutex_wait((__mutex_semaphore*) sem);

    uint64_t index = __vec->__vector_index;
    uint64_t element_size = __vec->__vector_element_size;
    uintptr_t tmp_ptr = (uintptr_t) __vec->__vector_ptr;
    if(likely(__vec->__vector_head_popped > 0)){
        memcpy((void*)tmp_ptr-element_size, (void*)tmp_ptr, element_size);
        --__vec->__vector_head_popped;
        ++__vec->__vector_index;
        __vec->__vector_ptr -= element_size;
        mutex_signal((__mutex_semaphore*) sem);
    }

    if(unlikely(index*element_size >= __vec->__vector_size)){
        if(vector_expand(__vec) < 0){mutex_signal((__mutex_semaphore*) sem); return -1;}
    }
    
    uint64_t i = 0;
    for(; i<index; i++){ //cannot do a unique memcpy because of pointer aliasing
        memcpy((void*)tmp_ptr+(element_size*(i+1)), (void*)tmp_ptr+(element_size*i), element_size);
    }

    memcpy((void*) tmp_ptr, __src, element_size);
    ++__vec->__vector_index;

    /* calculating exponential moving average of blocks accessed */
    uint32_t accessed_block = (((uint8_t*)tmp_ptr - __vec->__vector_ptr) / __PAGE_SIZE) + 1;
    uint32_t zero_swap_index = __vec->__vector_swap_indices[0];
    __vec->__vector_swap_indices[1] = zero_swap_index; 
    __vec->__vector_swap_indices[0] = (uint32_t)(accessed_block * __SWAP_GAMMA + (zero_swap_index * ((float)1-__SWAP_GAMMA)));
    
    vector_memcheck(__vec);

    mutex_signal((__mutex_semaphore*) sem);
    return 0;
} 


int vector_popn(Vector* restrict __vec, void* restrict __dest, const uint64_t __element_count){
    uintptr_t sem = (uintptr_t) &__vec->__vector_sem;
    
    mutex_wait((__mutex_semaphore*) sem);
    uint64_t index = __vec->__vector_index;
    if(unlikely(index - __element_count < 0)){
        mutex_signal((__mutex_semaphore*) sem);
        return -1;
    }
    uint64_t element_size = __vec->__vector_element_size;
    uintptr_t tmp_ptr = (uintptr_t) __vec->__vector_ptr + (index-1)*element_size;
    memcpy(__dest, (void*)tmp_ptr, element_size*__element_count);
    index -=  __element_count; __vec->__vector_index -= __element_count;
    if(unlikely(__vec->__vector_size - index > __PAGE_SIZE)) vector_shrink(__vec);
    
    /* calculating exponential moving average of blocks accessed */
    uint32_t accessed_block = (((uint8_t*)tmp_ptr - __vec->__vector_ptr) / __PAGE_SIZE) + 1;
    uint32_t zero_swap_index = __vec->__vector_swap_indices[0];
    __vec->__vector_swap_indices[1] = zero_swap_index; 
    __vec->__vector_swap_indices[0] = (uint32_t)(accessed_block * __SWAP_GAMMA + (zero_swap_index * ((float)1-__SWAP_GAMMA)));
    
    vector_memcheck(__vec);
    
    mutex_signal((__mutex_semaphore*) sem);
    return 0;
}

int vector_popf(Vector* restrict __vec, void* restrict __dest){
    uintptr_t sem = (uintptr_t) &__vec->__vector_sem;
    mutex_wait((__mutex_semaphore*) sem);
    uint64_t element_size = __vec->__vector_element_size;
    uint64_t index = __vec->__vector_index;
    if(unlikely(index == 0)){
        mutex_signal((__mutex_semaphore*) sem);
        return -1;
    }

    memcpy(__dest, (void*)__vec->__vector_ptr, element_size);
    if(likely(__vec->__vector_head_popped < __VECTOR_POPHEAD_DELAY)){
        --__vec->__vector_index;
        ++__vec->__vector_head_popped;
        __vec->__vector_ptr += element_size;
        mutex_signal((__mutex_semaphore*) sem);
        return 0;
    }

    uint64_t i = index-1;
    uintptr_t tmp_ptr = (uintptr_t) __vec->__vector_ptr;
    for(; i>=0; i--){ //cannot do a unique memcpy because of pointer aliasing
        memcpy((void*)tmp_ptr+(element_size*i), (void*)tmp_ptr+(element_size*(i+1)), element_size);
    }

    --__vec->__vector_index;
    __vec->__vector_head_popped = 0;
    __vec->__vector_ptr -= __VECTOR_POPHEAD_DELAY*element_size;


    /* calculating exponential moving average of blocks accessed */
    uint32_t accessed_block = (((uint8_t*)tmp_ptr - __vec->__vector_ptr) / __PAGE_SIZE) + 1;
    uint32_t zero_swap_index = __vec->__vector_swap_indices[0];
    __vec->__vector_swap_indices[1] = zero_swap_index; 
    __vec->__vector_swap_indices[0] = (uint32_t)(accessed_block * __SWAP_GAMMA + (zero_swap_index * ((float)1-__SWAP_GAMMA)));
    
    vector_memcheck(__vec);

    mutex_signal((__mutex_semaphore*) sem);
    return 0;

}


void* vector_at(Vector* restrict __vec, const uint64_t __index){
    uintptr_t sem = (uintptr_t) &__vec->__vector_sem;
    mutex_wait((__mutex_semaphore*) sem);
    if(unlikely(__index >= __vec->__vector_index)){
        mutex_signal((__mutex_semaphore*) sem);
        return (void*)-1;
    }

    uintptr_t addr = (uintptr_t)__vec->__vector_ptr + (__index*__vec->__vector_element_size);
    mutex_signal((__mutex_semaphore*) sem);

    return (void*)addr;
}