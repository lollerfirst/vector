#define _GNU_SOURCE

#include <stdint.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <memory.h>
#include <stdbool.h>
#include "semaphore.h"

#define __VECTOR_MAX_SIZE 1073741824  // 1 GB max size
#define __VECTOR_POPHEAD_DELAY 48
#define __SWAP_GAMMA 0.75F
#define __INIT_VALUE 46

#define unlikely(x) __builtin_expect(x, 0)
#define likely(x) __builtin_expect(x, 1)

#define check_init(__vec) (__vec->__vector_init)

#define vector_memcheck(__vec) {\
    if(unlikely(__vec->__vector_swap_indices[0] != __vec->__vector_swap_indices[1] && __vec->__vector_swap_indices[1] > 0)){\
    uintptr_t swap_addr1 = (uintptr_t) __vec->__vector_ptr + (((uint32_t)__vec->__vector_swap_indices[0]-1) * __PAGE_SIZE);\
    uintptr_t swap_addr2 = (uintptr_t) __vec->__vector_ptr + (((uint32_t)__vec->__vector_swap_indices[1]-1) * __PAGE_SIZE);\
    posix_madvise((void*)swap_addr1, __PAGE_SIZE, MADV_SEQUENTIAL);\
    posix_madvise((void*)swap_addr2, __PAGE_SIZE, MADV_DONTNEED);\
    }\
}


static uint32_t __PAGE_SIZE;

typedef unsigned __int128 uint128_t;

typedef struct __vector_type{ uint8_t __vector_init;
                         uint8_t* __vector_ptr; 
                         uint32_t __vector_pages;
                         float __vector_swap_indices[2];
                         uint64_t __vector_size; 
                         uint64_t __vector_element_size; 
                         uint64_t __vector_index;
                         uint16_t __vector_head_popped;
                         __mutex_semaphore __vector_sem; 
                         } Vector;


/* Read __element_count elements starting from __index (ATTENTION: this can still segfault)*/
void vector_readn(Vector* restrict __vec, void* restrict __dest, const uint64_t __index, const size_t __element_count);
/* Store __element_count elements starting from __index (ATTENTION: this can still segfault)*/
void vector_storen(Vector*  restrict __vec, void*  restrict __src, const uint64_t __index, const size_t __element_count);
/* Initialize the vector with specified __size and of the type __type_size */
int vector_init(Vector* __vec, const size_t __size, const size_t __type_size);
/* Push __element_count elements to the back of the vector from __src */
int vector_pushn(Vector* restrict __vec, const void*  restrict __src, const size_t __element_count);
/* Push an element to the front of the vector from __src */
int vector_pushf(Vector* restrict __vec, const void* restrict __src);
/* Pop __element_count elements from the back of the vector into __dest */
int vector_popn(Vector* restrict __vec, void* restrict __dest, const size_t __element_count);
/* Pop an element from the front of the vector into __dest */
int vector_popf(Vector* restrict __vec, void* restrict __dest);
/* Returns the address of an element at the specified zero addressed index */
void* vector_at(Vector* __vec, const uint64_t __index);
/* Frees the memory and deinitializes the vector*/
void vector_free(Vector* __vec);

static inline int vector_expand(Vector*);
static inline int vector_shrink(Vector*);
 
int vector_init(Vector* __vec, const size_t __size, const size_t __type_size){
    __PAGE_SIZE = (uint32_t) sysconf(_SC_PAGESIZE);
    
    if (check_init(__vec) == __INIT_VALUE) return -1;

    uint8_t* tmp_ptr;
    if((tmp_ptr = mmap(NULL, (((__size*__type_size) / __PAGE_SIZE) + 1)* __PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) == (uint8_t*)-1 || (uint128_t)__size * __type_size >= __VECTOR_MAX_SIZE) return -1;

    uint16_t tsize =  (__size*__type_size / __PAGE_SIZE) + 1;
    mutex_init(&__vec->__vector_sem);

    memset(__vec, 0, sizeof(Vector));
    __vec->__vector_ptr = tmp_ptr;
    __vec->__vector_pages = tsize;
    __vec->__vector_swap_indices[0] = 1;
    __vec->__vector_size = tsize * __PAGE_SIZE;
    __vec->__vector_element_size = __type_size;
    __vec->__vector_init = __INIT_VALUE;

    posix_madvise((void*) tmp_ptr, tsize * __PAGE_SIZE, POSIX_MADV_SEQUENTIAL);
    return 0;
}


static inline int vector_expand(Vector* __vec){
    uint8_t* tmp_ptr;

    uint16_t add_type_pages = (__vec->__vector_element_size / __PAGE_SIZE) + 1;
    if((tmp_ptr = mremap((void*)__vec->__vector_ptr, __vec->__vector_size, __vec->__vector_size + (__PAGE_SIZE*add_type_pages), MREMAP_MAYMOVE)) == (void*)-1) return -1;
    
    __vec->__vector_size += __PAGE_SIZE*add_type_pages;
    __vec->__vector_pages += add_type_pages;
    __vec->__vector_ptr = tmp_ptr;
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

    return 0;
}

void vector_readn(Vector* restrict __vec, void* restrict __dest, const uint64_t __index, const size_t __element_count){
    uintptr_t sem = (uintptr_t) &__vec->__vector_sem;
    mutex_wait((__mutex_semaphore*) sem);

    if (unlikely(!(check_init(__vec) == __INIT_VALUE))){
        mutex_signal((__mutex_semaphore*) sem);
        __dest = NULL;
        return;
    }


    uint64_t element_size = __vec->__vector_element_size;
    uintptr_t tmp_ptr = (uintptr_t) __vec->__vector_ptr + (__index*element_size);
    memcpy(__dest, (void*)tmp_ptr, element_size*__element_count);

    /* calculating exponential moving average of blocks accessed */
    uint32_t accessed_block = (((uint8_t*)tmp_ptr - __vec->__vector_ptr) / __PAGE_SIZE) + 1;
    float zero_swap_index = __vec->__vector_swap_indices[0];
    __vec->__vector_swap_indices[1] = zero_swap_index; 
    __vec->__vector_swap_indices[0] = accessed_block * __SWAP_GAMMA + (zero_swap_index * (1.0F -__SWAP_GAMMA));
    
    vector_memcheck(__vec);
    mutex_signal((__mutex_semaphore*) sem);
}  


void vector_storen(Vector*  restrict __vec, void*  restrict __src, const uint64_t __index, const size_t __element_count){
    uintptr_t sem = (uintptr_t) &__vec->__vector_sem;
    mutex_wait((__mutex_semaphore*) sem);
    
    if (unlikely(!(check_init(__vec) == __INIT_VALUE))){
        mutex_signal((__mutex_semaphore*) sem);
        return;
    }

    uint64_t element_size = __vec->__vector_element_size;
    uintptr_t tmp_ptr = (uintptr_t) __vec->__vector_ptr + (__index*element_size);
    memcpy((void*)tmp_ptr, __src, element_size*__element_count);

    /* calculating exponential moving average of blocks accessed */
    uint32_t accessed_block = (((uint8_t*)tmp_ptr - __vec->__vector_ptr) / __PAGE_SIZE) + 1;
    float zero_swap_index = __vec->__vector_swap_indices[0];
    __vec->__vector_swap_indices[1] = zero_swap_index; 
    __vec->__vector_swap_indices[0] = accessed_block * __SWAP_GAMMA + (zero_swap_index * (1.0F - __SWAP_GAMMA));
    
    vector_memcheck(__vec);
    mutex_signal((__mutex_semaphore*) sem);
}


int vector_pushn(Vector* restrict __vec, const void*  restrict __src, const size_t __element_count){
    uintptr_t sem = (uintptr_t) &__vec->__vector_sem;
    mutex_wait((__mutex_semaphore*) sem);

    if (unlikely((!check_init(__vec) == __INIT_VALUE))){
        mutex_signal((__mutex_semaphore*) sem);
        return -1;
    }

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
    float zero_swap_index = __vec->__vector_swap_indices[0];
    __vec->__vector_swap_indices[1] = zero_swap_index; 
    __vec->__vector_swap_indices[0] = accessed_block * __SWAP_GAMMA + (zero_swap_index * (1.0F - __SWAP_GAMMA));
    
    vector_memcheck(__vec);

    mutex_signal((__mutex_semaphore*) sem);
    return 0;
}

int vector_pushf(Vector* restrict __vec, const void* restrict __src){
    uintptr_t sem = (uintptr_t) &__vec->__vector_sem;
    mutex_wait((__mutex_semaphore*) sem);

    if (unlikely((!check_init(__vec) == __INIT_VALUE))){
        mutex_signal((__mutex_semaphore*) sem);
        return -1;
    }

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
    float zero_swap_index = __vec->__vector_swap_indices[0];
    __vec->__vector_swap_indices[1] = zero_swap_index; 
    __vec->__vector_swap_indices[0] = accessed_block * __SWAP_GAMMA + (zero_swap_index * (1.0F - __SWAP_GAMMA));
    
    vector_memcheck(__vec);

    mutex_signal((__mutex_semaphore*) sem);
    return 0;
} 


int vector_popn(Vector* restrict __vec, void* restrict __dest, const size_t __element_count){
    uintptr_t sem = (uintptr_t) &__vec->__vector_sem;
    
    mutex_wait((__mutex_semaphore*) sem);

    if (unlikely(!(check_init(__vec) == __INIT_VALUE))){
        mutex_signal((__mutex_semaphore*) sem);
        return -1;
    }

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
    float zero_swap_index = __vec->__vector_swap_indices[0];
    __vec->__vector_swap_indices[1] = zero_swap_index; 
    __vec->__vector_swap_indices[0] = accessed_block * __SWAP_GAMMA + (zero_swap_index * (1.0F - __SWAP_GAMMA));
    
    vector_memcheck(__vec);
    
    mutex_signal((__mutex_semaphore*) sem);
    return 0;
}

int vector_popf(Vector* restrict __vec, void* restrict __dest){

    uintptr_t sem = (uintptr_t) &__vec->__vector_sem;
    mutex_wait((__mutex_semaphore*) sem);

    if ((unlikely(!(check_init(__vec) == __INIT_VALUE)))){
        mutex_signal((__mutex_semaphore*) sem);
        return -1;
    }

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
    float zero_swap_index = __vec->__vector_swap_indices[0];
    __vec->__vector_swap_indices[1] = zero_swap_index; 
    __vec->__vector_swap_indices[0] = accessed_block * __SWAP_GAMMA + (zero_swap_index * (1.0F - __SWAP_GAMMA));
    
    vector_memcheck(__vec);

    mutex_signal((__mutex_semaphore*) sem);
    return 0;

}


void* vector_at(Vector* restrict __vec, const uint64_t __index){
    uintptr_t sem = (uintptr_t) &__vec->__vector_sem;
    mutex_wait((__mutex_semaphore*) sem);

    if (unlikely(!(check_init(__vec) == __INIT_VALUE))){
        mutex_signal((__mutex_semaphore*) sem);
        return NULL;
    }

    if(unlikely(__index >= __vec->__vector_index)){
        mutex_signal((__mutex_semaphore*) sem);
        return NULL;
    }

    uintptr_t addr = (uintptr_t)__vec->__vector_ptr + (__index*__vec->__vector_element_size);
    mutex_signal((__mutex_semaphore*) sem);

    return (void*)addr;
}


void vector_free(Vector* __vec){
    uintptr_t sem = (uintptr_t) &__vec->__vector_sem;
    mutex_wait((__mutex_semaphore*) sem);

    if (unlikely(!(check_init(__vec) == __INIT_VALUE))){
        mutex_signal((__mutex_semaphore*) sem);
        return;
    }
    
    munmap(__vec->__vector_ptr, __vec->__vector_pages*__PAGE_SIZE);
    memset(__vec, 0, sizeof(Vector));
}